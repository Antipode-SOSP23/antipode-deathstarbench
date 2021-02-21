#!/bin/bash
echo "*********************************"
echo "Build RabbitMQ Federation"
echo "*********************************"

sleep 60 | echo "Sleeping"

# test connection with
# rabbitmqctl -n rabbit@<server> environment

# federate write-home-timeline-rabbitmq-us to write-home-timeline-rabbitmq-eu
config='{"max-hops": 1, "uri": ["amqp://admin:admin@write-home-timeline-rabbitmq-eu"]}'
rabbitmqctl -n rabbit@write-home-timeline-rabbitmq-us set_parameter federation-upstream cluster1 "${config}"
config='[{"upstream": "cluster1"}]'
rabbitmqctl -n rabbit@write-home-timeline-rabbitmq-us set_parameter federation-upstream-set cluster1_federators "${config}"
config='{"federation-upstream-set": "cluster1_federators"}'
rabbitmqctl -n rabbit@write-home-timeline-rabbitmq-us set_policy --apply-to exchanges federation_test "write\-home\-timeline\-*" "${config}"
config='{"ha-mode": "all"}'
rabbitmqctl -n rabbit@write-home-timeline-rabbitmq-us set_policy ha-federation "^federation:*" "${config}"

# federate write-home-timeline-rabbitmq-eu to write-home-timeline-rabbitmq-us
config='{"max-hops": 1, "uri": ["amqp://admin:admin@write-home-timeline-rabbitmq-us"]}'
rabbitmqctl -n rabbit@write-home-timeline-rabbitmq-eu set_parameter federation-upstream cluster2 "${config}"
config='[{"upstream": "cluster2"}]'
rabbitmqctl -n rabbit@write-home-timeline-rabbitmq-eu set_parameter federation-upstream-set cluster2_federators "${config}"
config='{"federation-upstream-set": "cluster2_federators"}'
rabbitmqctl -n rabbit@write-home-timeline-rabbitmq-eu set_policy --apply-to exchanges federation_test "write\-home\-timeline\-*" "${config}"
config='{"ha-mode": "all"}'
rabbitmqctl -n rabbit@write-home-timeline-rabbitmq-eu set_policy ha-federation "^federation:*" "${config}"

# restart servers?
# rabbitmqctl -n rabbit@write-home-timeline-rabbitmq-us stop
# rabbitmqctl -n rabbit@write-home-timeline-rabbitmq-eu stop
# rabbitmqctl -n rabbit@write-home-timeline-rabbitmq-us start_app
# rabbitmqctl -n rabbit@write-home-timeline-rabbitmq-eu start_app


echo "*********************************"
echo "RabbitMQ Federation DONE!"
echo "*********************************"