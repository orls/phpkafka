/**
 *  Copyright 2013-2014 Patrick Reilly.
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <php.h>
#include <php_kafka.h>
#include "kafka.h"
//add exception header
#include "zend_exceptions.h"

/* decalre the class entry */
zend_class_entry *kafka_ce;

/* the method table */
/* each method can have its own parameters and visibility */
static zend_function_entry kafka_functions[] = {
    PHP_ME(Kafka, __construct, NULL, ZEND_ACC_CTOR | ZEND_ACC_PUBLIC)
    PHP_ME(Kafka, setBrokers, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Kafka, disconnect, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Kafka, isConnected, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Kafka, set_partition, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Kafka, setPartition, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Kafka, produce, NULL, ZEND_ACC_PUBLIC)
    PHP_ME(Kafka, consume, NULL, ZEND_ACC_PUBLIC)
    {NULL,NULL,NULL} /* Marks the end of function entries */
};

zend_module_entry kafka_module_entry = {
    STANDARD_MODULE_HEADER,
    "kafka",
    kafka_functions, /* Function entries */
    PHP_MINIT(kafka), /* Module init */
    PHP_MSHUTDOWN(kafka), /* Module shutdown */
    PHP_RINIT(kafka), /* Request init */
    PHP_RSHUTDOWN(kafka), /* Request shutdown */
    NULL, /* Module information */
    PHP_KAFKA_VERSION, /* Replace with version number for your extension */
    STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_KAFKA
ZEND_GET_MODULE(kafka)
#endif


PHP_MINIT_FUNCTION(kafka)
{
    zend_class_entry ce;
    INIT_CLASS_ENTRY(ce, "Kafka", kafka_functions);
    kafka_ce = zend_register_internal_class(&ce TSRMLS_CC);
    //add connected flag/property
    zend_declare_property_bool(
        kafka_ce, "connected", sizeof("connected")-1, 0, ZEND_ACC_PRIVATE TSRMLS_CC
    );
    //keep track of the brokers DSN
    zend_declare_property_null(kafka_ce, "brokers", sizeof("brokers") -1, ZEND_ACC_PRIVATE TSRMLS_CC);
    return SUCCESS;
}
PHP_RSHUTDOWN_FUNCTION(kafka) { return SUCCESS; }
PHP_RINIT_FUNCTION(kafka) { return SUCCESS; }
PHP_MSHUTDOWN_FUNCTION(kafka) {
    kafka_destroy();
    return SUCCESS;
}

PHP_METHOD(Kafka, __construct)
{
    char *brokers = "localhost:9092";
    int brokers_len;
    zval *this_obj = getThis();

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|s",
            &brokers, &brokers_len) == FAILURE) {
        return;
    }
    //set brokers property to correct value
    zend_update_property_stringl(
        kafka_ce, this_obj, "brokers", sizeof("brokers") -1,
        brokers, brokers_len TSRMLS_DC
    );
    kafka_connect(brokers);
    kafka_init_connection(0);
    //update connected flag
    zend_update_property_bool(
        kafka_ce, this_obj, "connected", sizeof("connected") -1,
        1 TSRMLS_DC
    );
}

PHP_METHOD(Kafka, setBrokers)
{
    char *brokers;
    int brokers_len;
    zval *this_obj,
        *is_connected,
        *zbrokers;
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s",
        &brokers, &brokers_len) == FAILURE) {
        RETURN_FALSE;
    }
    this_obj = getThis();
    is_connected = zend_read_property(kafka_ce, this_obj, "connected", sizeof("connected") - 1, 0 TSRMLS_CC);
    if (Z_LVAL_P(is_connected)) {
        //Kafka connection needs to be closed, before we can connect to a new one
        //but first, check the brokers value:
        zbrokers = zend_read_property(kafka_ce,this_obj, "brokers", sizeof("brokers") -1, 0 TSRMLS_CC);
        if (brokers_len == Z_STRLEN_P(zbrokers) && strcmp(brokers, Z_STRVAL_P(zbrokers)) == 0) {
            RETURN_TRUE;
        }
        kafka_destroy();
        //set connected to false
        zend_update_property_bool(kafka_ce, this_obj, "connected", sizeof("connected") -1, 0 TSRMLS_DC);
    }
    zend_update_property_stringl(
        kafka_ce, this_obj, "brokers", sizeof("brokers") -1,
        brokers, brokers_len TSRMLS_DC
    );
    kafka_connect(brokers);
    kafka_init_connection(0);
    //update connected flag
    zend_update_property_bool(
        kafka_ce, this_obj, "connected", sizeof("connected") -1,
        1 TSRMLS_DC
    );
}

PHP_METHOD(Kafka, disconnect)
{
    zval *this_obj = getThis(),
        *is_connected;
    is_connected = zend_read_property(kafka_ce, this_obj, "connected", sizeof("connected") -1, 0 TSRMLS_CC);
    if (Z_LVAL_P(is_connected)) {
        kafka_destroy();
        zend_update_property_bool(
            kafka_ce, this_obj, "connected", sizeof("connected") -1, 0 TSRMLS_DC
        );
        zend_update_property_null(
            kafka_ce, this_obj, "brokers", sizeof("brokers") -1 TSRMLS_DC
        );
    }
    RETURN_TRUE;
}

PHP_METHOD(Kafka, isConnected)
{
    zval *this_obj = getThis(),
        *is_connected;
    is_connected = zend_read_property(kafka_ce, this_obj, "connected", sizeof("connected") -1, 0 TSRMLS_CC);
    RETURN_ZVAL(is_connected, 1, 0);
}

//Duplicate method, to conform to the PSR standards
PHP_METHOD(Kafka, setPartition)
{
    zval *object = getThis(),
        *is_connected,
        *partition;
    is_connected = zend_read_property(kafka_ce, object, "connected", sizeof("connected") -1, 0 TSRMLS_CC);
    if (Z_LVAL_P(is_connected) == 0) {
        //TODO: throw exceptions in these cases!
        RETURN_FALSE;
    }
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|z",
            &partition) == FAILURE) {
        return;
    }

    if (Z_TYPE_P(partition) == IS_LONG) {
        kafka_set_partition(Z_LVAL_P(partition));
    }

}

PHP_METHOD(Kafka, set_partition)
{
    zval *partition;

    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|z",
            &partition) == FAILURE) {
        return;
    }

    if (Z_TYPE_P(partition) == IS_LONG) {
        kafka_set_partition(Z_LVAL_P(partition));
    }
}

PHP_METHOD(Kafka, produce)
{
    zval *object = getThis(),
        *is_connected;
    char *topic;
    char *msg;
    int topic_len;
    int msg_len;
    is_connected = zend_read_property(kafka_ce, object, "connected", sizeof("connected") -1, 0 TSRMLS_CC);
    if (Z_LVAL_P(is_connected) == 0) {
        //TODO: throw exceptions in these cases!
        RETURN_FALSE;
    }
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "ss",
            &topic, &topic_len,
            &msg, &msg_len) == FAILURE) {
        return;
    }

    kafka_produce(topic, msg, msg_len);

    RETURN_TRUE;
}

PHP_METHOD(Kafka, consume)
{
    zval *object = getThis(),
        *is_connected;
    char *topic;
    int topic_len;
    char *offset;
    int offset_len;
    long item_count = 0;

    is_connected = zend_read_property(kafka_ce, object, "connected", sizeof("connected") -1, 0 TSRMLS_CC);
    if (Z_LVAL_P(is_connected) == 0) {
        //TODO: throw exceptions in these cases!
        RETURN_FALSE;
    }
    if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "s|sl",
            &topic, &topic_len,
            &offset, &offset_len,
            &item_count) == FAILURE) {
        return;
    }

    array_init(return_value);
    kafka_consume(return_value, topic, offset, item_count);

    if(return_value == NULL) {
        RETURN_FALSE;
    }
}
