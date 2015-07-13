<?php
final class KafkaTopic
{
    /**
     * internal property storing the name of the topic
     * @var string
     */
    private $name = null;

    /**
     * mode in which this instance operates Kafka::MODE_* constants
     * @var int
     */
    private $mode = null;

    /**
     * Internal member: metadata, initialized when needed
     */
    private $meta = null;

    /**
     * Part of the internal metadata struct
     * @var int
     */
    private $partitionCount = 0;

    /**
     * Can be used, but is discouraged, use Kafka::getTopic instead
     * The Kafka::getTopic method will connect if required
     * Calling the constructor directly (without valid connections) will throw exceptions
     * @param Kafka $connection
     * @param string $topicName
     * @param int $mode
     * @throws KafkaException
     */
    public function __construct(Kafka $connection, $topicName, $mode)
    {
        $this->name = $topicName;
        $this->mode = $mode;
    }

    /**
     * @return string
     */
    public function getName()
    {
        return $this->name;
    }

    /**
     * This method doesn't exist
     * represents initialization of topic metadata
     * @return void
     */
    private function initMeta()
    {
        $this->meta = [
            'topics' => [
                'partition_count'   => 0,
            ],
        ];
        return $this;
    }

    /**
     * @return int
     * @throws \KafkaException
     */
    public function getPartitionCount()
    {
        if (!$this->meta)
        {
            $this->initMeta();
        }
        if (!$this->meta)
        {//init failed
            throw new \KafkaException('failed to fetch metadata for topic');
        }
        $this->partitionCount = $this->meta['topics']['partition_count'];
        return $this->partitionCount;
    }
}
