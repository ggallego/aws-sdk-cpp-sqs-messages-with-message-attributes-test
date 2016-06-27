#include <aws/core/Aws.h>
#include <aws/sqs/SQSClient.h>
#include <aws/sqs/model/CreateQueueRequest.h>
#include <aws/sqs/model/SendMessageRequest.h>
#include <aws/sqs/model/ReceiveMessageRequest.h>
#include <aws/sqs/model/DeleteMessageRequest.h>
#include <aws/sqs/model/DeleteQueueRequest.h>
#include <aws/sqs/model/DeleteMessageBatchRequest.h>
#include <aws/sqs/model/DeleteMessageBatchRequestEntry.h>
#include <aws/sqs/model/SendMessageBatchRequest.h>
#include <aws/sqs/model/SendMessageBatchRequestEntry.h>
#include <aws/sqs/model/ListQueuesRequest.h>
#include "gtest/gtest.h"

using namespace Aws;
using namespace Aws::Client;
using namespace Aws::Http;
using namespace Aws::SQS;
using namespace Aws::SQS::Model;

static const char* ALLOCATION_TAG = "SQSBatchWithMessageAttributesTest";

#define TEST_QUEUE_PREFIX "IntegrationTest_"
static const char* SIMPLE_QUEUE_NAME = TEST_QUEUE_PREFIX "SimpleQueue";
static const char* BATCH_QUEUE_NAME = TEST_QUEUE_PREFIX "BatchQueue";

static const char* CUSTOM_MESSAGE_ATTRIBUTE_NAME = "CustomMessageAttribute";
static const char* CUSTOM_MESSAGE_ATTRIBUTE_VALUE = "CustomMessageAttributeValue";

namespace
{

  class SQSBatchWithMessageAttributesTest : public testing::Test
  {

  public:
    std::shared_ptr<SQSClient> sqsClient;

  protected:

    virtual void SetUp ()
    {
      ClientConfiguration config;
      config.scheme = Scheme::HTTPS;
      config.region = Region::US_EAST_1;
#if USE_PROXY_FOR_TESTS
      config.scheme = Scheme::HTTP;
      config.proxyHost = PROXY_HOST;
      config.proxyPort = PROXY_PORT;
#endif
      sqsClient = Aws::MakeShared<SQSClient> (ALLOCATION_TAG, config);

      // delete queues, just in case
      DeleteAllTestQueues();
    }

    virtual void TearDown ()
    {
      // delete queues, just in case
      DeleteAllTestQueues();
      sqsClient = nullptr;
    }

    void DeleteAllTestQueues()
    {
        ListQueuesRequest listQueueRequest;
        listQueueRequest.WithQueueNamePrefix(TEST_QUEUE_PREFIX);

        ListQueuesOutcome listQueuesOutcome = sqsClient->ListQueues(listQueueRequest);
        ListQueuesResult listQueuesResult = listQueuesOutcome.GetResult();
        Aws::Vector<Aws::String> urls = listQueuesResult.GetQueueUrls();
        for (auto& url : listQueuesResult.GetQueueUrls())
        {
            DeleteQueueRequest deleteQueueRequest;
            deleteQueueRequest.WithQueueUrl(url);
            DeleteQueueOutcome deleteQueueOutcome = sqsClient->DeleteQueue(deleteQueueRequest);
        }

        bool done = false;
        while(!done)
        {
            listQueuesOutcome = sqsClient->ListQueues(listQueueRequest);
            listQueuesResult = listQueuesOutcome.GetResult();
            if(listQueuesResult.GetQueueUrls().size() == 0)
            {
                break;
            }

            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
  };
} // anonymous namespace


TEST_F(SQSBatchWithMessageAttributesTest, simpleTest)
{
  // create queue
  CreateQueueRequest request;
  request.SetQueueName (SIMPLE_QUEUE_NAME);
  CreateQueueOutcome outcome = sqsClient->CreateQueue (request);
  Aws::String queueUrl = outcome.GetResult ().GetQueueUrl ();

  // send message
  SendMessageRequest sendMessageRequest;
  sendMessageRequest.SetQueueUrl (queueUrl);
  Aws::String messageBody = Aws::String (10, 'x');;
  sendMessageRequest.SetMessageBody (messageBody);
  MessageAttributeValue messageAttributeValue;
  messageAttributeValue.SetDataType ("String");
  messageAttributeValue.SetStringValue (CUSTOM_MESSAGE_ATTRIBUTE_VALUE);
  sendMessageRequest.AddMessageAttributes (CUSTOM_MESSAGE_ATTRIBUTE_NAME, messageAttributeValue);
  SendMessageOutcome sendM = sqsClient->SendMessage (sendMessageRequest);
  ASSERT_TRUE(sendM.IsSuccess ());
  EXPECT_TRUE(sendM.GetResult ().GetMessageId ().length () > 0);

  // receive message
  ReceiveMessageRequest receiveMessageRequest;
  receiveMessageRequest.SetMaxNumberOfMessages (1);
  receiveMessageRequest.SetQueueUrl (queueUrl);
  receiveMessageRequest.AddMessageAttributeNames("All");
  ReceiveMessageOutcome receiveM = sqsClient->ReceiveMessage (receiveMessageRequest);
  ASSERT_TRUE(receiveM.IsSuccess ());
  ASSERT_EQ(1uL, receiveM.GetResult ().GetMessages ().size ());
  EXPECT_EQ(messageBody, receiveM.GetResult ().GetMessages ()[0].GetBody ());
  Aws::Map<Aws::String, MessageAttributeValue> messageAttributes = receiveM.GetResult ().GetMessages ()[0].GetMessageAttributes ();
  ASSERT_EQ(1ul, messageAttributes.size());
  ASSERT_TRUE(messageAttributes.find(CUSTOM_MESSAGE_ATTRIBUTE_NAME) != messageAttributes.end());
  EXPECT_EQ(messageAttributeValue.GetStringValue(), messageAttributes[CUSTOM_MESSAGE_ATTRIBUTE_NAME].GetStringValue());

  // delete message
  DeleteMessageRequest deleteMessageRequest;
  deleteMessageRequest.SetQueueUrl (queueUrl);
  Aws::String receiptHandle = receiveM.GetResult ().GetMessages ()[0].GetReceiptHandle ();
  deleteMessageRequest.SetReceiptHandle (receiptHandle);
  DeleteMessageOutcome deleteM = sqsClient->DeleteMessage (deleteMessageRequest);
  ASSERT_TRUE(deleteM.IsSuccess ());
  receiveM = sqsClient->ReceiveMessage (receiveMessageRequest);
  EXPECT_EQ(0uL, receiveM.GetResult ().GetMessages ().size ());

  // delete queue
  DeleteQueueRequest deleteQueueRequest;
  deleteQueueRequest.SetQueueUrl (queueUrl);
  DeleteQueueOutcome deleteQ = sqsClient->DeleteQueue (deleteQueueRequest);
  ASSERT_TRUE(deleteQ.IsSuccess ());

}

TEST_F(SQSBatchWithMessageAttributesTest, batchTest)
{
  // create queue
  CreateQueueRequest request;
  request.SetQueueName (BATCH_QUEUE_NAME);
  CreateQueueOutcome outcome = sqsClient->CreateQueue (request);
  Aws::String queueUrl = outcome.GetResult ().GetQueueUrl ();

  // build a sendbatch message
  SendMessageBatchRequestEntry sendEntry;
  Aws::String messageBody = Aws::String (10, 'x');
  sendEntry.SetMessageBody (messageBody);
  sendEntry.SetId ("1");
  MessageAttributeValue messageAttributeValue;
  messageAttributeValue.SetDataType ("String");
  messageAttributeValue.SetStringValue (CUSTOM_MESSAGE_ATTRIBUTE_VALUE);
  sendEntry.AddMessageAttributes (CUSTOM_MESSAGE_ATTRIBUTE_NAME, messageAttributeValue);

  // send message
  SendMessageBatchRequest sendMessageBatchRequest;
  sendMessageBatchRequest.SetQueueUrl (queueUrl);
  sendMessageBatchRequest.AddEntries (sendEntry);
  SendMessageBatchOutcome sendM = sqsClient->SendMessageBatch (sendMessageBatchRequest);
  ASSERT_TRUE(sendM.IsSuccess ());
  ASSERT_EQ(1, sendM.GetResult ().GetSuccessful ().size ());

  // receive message
  ReceiveMessageRequest receiveMessageRequest;
  receiveMessageRequest.SetMaxNumberOfMessages (1);
  receiveMessageRequest.SetQueueUrl (queueUrl);
  receiveMessageRequest.AddMessageAttributeNames("All");
  ReceiveMessageOutcome receiveM = sqsClient->ReceiveMessage (receiveMessageRequest);
  ASSERT_TRUE(receiveM.IsSuccess ());
  ASSERT_EQ(1uL, receiveM.GetResult ().GetMessages ().size ());
  EXPECT_EQ(messageBody, receiveM.GetResult ().GetMessages ()[0].GetBody ());
  Aws::Map<Aws::String, MessageAttributeValue> messageAttributes = receiveM.GetResult ().GetMessages ()[0].GetMessageAttributes ();
  ASSERT_EQ(1ul, messageAttributes.size());
  ASSERT_TRUE(messageAttributes.find(CUSTOM_MESSAGE_ATTRIBUTE_NAME) != messageAttributes.end());
  EXPECT_EQ(messageAttributeValue.GetStringValue(), messageAttributes[CUSTOM_MESSAGE_ATTRIBUTE_NAME].GetStringValue());

  // build a deletebatch entry
  DeleteMessageBatchRequestEntry deleteEntry;
  Aws::String receiptHandle = receiveM.GetResult ().GetMessages ()[0].GetReceiptHandle ();
  deleteEntry.SetReceiptHandle (receiptHandle);
  deleteEntry.SetId ("1");

  // delete messages
  DeleteMessageBatchRequest deleteMessageBatchRequest;
  deleteMessageBatchRequest.SetQueueUrl (queueUrl);
  deleteMessageBatchRequest.AddEntries (deleteEntry);
  DeleteMessageBatchOutcome deleteM = sqsClient->DeleteMessageBatch (deleteMessageBatchRequest);
  ASSERT_TRUE(deleteM.IsSuccess ());
  receiveM = sqsClient->ReceiveMessage (receiveMessageRequest);
  EXPECT_EQ(0uL, receiveM.GetResult ().GetMessages ().size ());

  // delete queue
  DeleteQueueRequest deleteQueueRequest;
  deleteQueueRequest.SetQueueUrl (queueUrl);
  DeleteQueueOutcome deleteQ = sqsClient->DeleteQueue (deleteQueueRequest);
  ASSERT_TRUE(deleteQ.IsSuccess ());

}
