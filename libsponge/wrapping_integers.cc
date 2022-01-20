#include "wrapping_integers.hh"

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) {
    // uint64 to uint32
    uint32_t offset = n & 0xFFFFFFFF;
    return WrappingInt32{isn + offset};
}

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    // change the checkpoint to a seqno
    uint32_t offset = n - isn; // the absolute seqno
    if (checkpoint <= offset) {
        return offset;
    }
    // find the closest offset t checkpoint
    uint64_t overflow_part = (checkpoint - offset) / (1ul << 32);
    uint64_t left_closest = offset + overflow_part * (1ul << 32);
    uint64_t right_closest = left_closest + (1ul << 32);
    if (checkpoint - left_closest < right_closest - checkpoint) {
        return left_closest;
    }
    return right_closest;
}
