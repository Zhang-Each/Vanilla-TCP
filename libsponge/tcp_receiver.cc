#include "tcp_receiver.hh"
#include <optional>

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    WrappingInt32 seqno = seg.header().seqno;
    if (_isn == nullptr) {
        if (seg.header().syn == true) {
            WrappingInt32 seq(seqno);
            _isn.reset(new WrappingInt32(seq));
            _reassembler.push_substring(string(seg.payload().str()), 0, seg.header().fin);
        }
    } else {
        uint64_t abs_seqno = unwrap(seqno, *_isn, _reassembler.stream_out().bytes_written() + 1);
        if (abs_seqno == 0) {
            return;
        }
        uint64_t index = abs_seqno - 1;
        string data = string(seg.payload().str());
        _reassembler.push_substring(data, index, seg.header().fin);
    }
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (_isn == nullptr) {
        return nullopt;
    } else {
        uint64_t n = _reassembler.stream_out().bytes_written() + 1;
        n += static_cast<uint64_t>(_reassembler.stream_out().input_ended());
        return wrap(n, *_isn);
    }
}

size_t TCPReceiver::window_size() const {
    return _capacity - _reassembler.stream_out().buffer_size();
}
