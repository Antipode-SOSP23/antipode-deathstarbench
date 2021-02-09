#!/usr/bin/env python3
import pika
from datetime import datetime

QUEUE = 'write-home-timeline'
creds = pika.PlainCredentials('admin', 'admin')
message = str(datetime.now())

#------------------------------
# Publish message on Rabbit1
#------------------------------
rabbit1 = pika.ConnectionParameters(host='localhost', port=5672, credentials=creds)

# create connection and channel
rabbit1_connection = pika.BlockingConnection([rabbit1])
rabbit1_channel = rabbit1_connection.channel()

# publish message
rabbit1_channel.basic_publish(exchange='', routing_key=QUEUE, body=message)

# close connection
rabbit1_channel.close()
rabbit1_connection.close()


#------------------------------
# Receive message on Rabbit2
#------------------------------
rabbit2 = pika.ConnectionParameters(host='localhost', port=5673, credentials=creds)

# create connection and channel
rabbit2_connection = pika.BlockingConnection(rabbit2)
rabbit2_channel = rabbit2_connection.channel()

queue = rabbit2_channel.queue_declare(queue=QUEUE, durable=True, exclusive=False, auto_delete=False)
print(queue.method.message_count)

# receive message
_, _, resp_body = rabbit2_channel.basic_get(queue=QUEUE, auto_ack=True)
assert resp_body.decode() == message

# close connection
rabbit2_channel.close()
rabbit2_connection.close()

print("[INFO] Test successful!")
