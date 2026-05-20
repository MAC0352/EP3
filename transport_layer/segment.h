struct segment
{
    int sourcePort;
    int destinationPort;
    int sequencyNumber;
    int AckNumber;
    int checksum;
    int windowSize;
    int flag;
    void* data;
};
