struct Packet
{
    int headerLength;
    int totalLength;
    int sourceAddress;
    int destinationAddress;
    void* data;

};
