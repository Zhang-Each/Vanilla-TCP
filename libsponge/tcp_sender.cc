#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _rto(_initial_retransmission_timeout) {}

void TCPSender::fill_window() {
    while (true) {
        bool syn =(_next_seqno == 0);
        bool fin = _stream.input_ended() && _stream.buffer_size() <= TCPConfig::MAX_PAYLOAD_SIZE &&
                   (static_cast<int64_t>(_stream.buffer_size()) <= static_cast<int64_t>(*_last_window_size) - syn - 1);
        // size of the payload to fill the window.
        uint64_t payload_size = std::min({TCPConfig::MAX_PAYLOAD_SIZE,
                                      _stream.buffer_size(),
                                      static_cast<uint64_t>(*_last_window_size - syn - fin)});
        TCPSegment seg;
        seg.header().seqno = wrap(_next_seqno, _isn);
        seg.header().syn = syn;
        seg.header().fin = fin;
        seg.payload() = Buffer( _stream.read(payload_size));
        if (_fin_sent || (syn == false && fin == false && payload_size == 0)) {
            return;
        }
        if (fin) {
            _fin_sent = true;
        }
        // send out the TCP segment.
        _segments_out.push(seg);
        _outstanding_segs.emplace(_next_seqno, seg);
        size_t seg_length = seg.length_in_sequence_space();
        _bytes_in_flight += seg_length;
        _next_seqno += seg_length;
        *_last_window_size -= seg_length;
        if (!_timer_ms.has_value()) {
            _timer_ms = _ms;
        }
    }
    
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    uint64_t ack = unwrap(ackno, _isn, _last_ackno.has_value() ? *_last_ackno : 0);
    bool new_data = false;
    if (_last_ackno.has_value()) {
        if (_last_ackno <= ack && ack <= _next_seqno) {
            new_data = ack > _last_ackno;
            _last_ackno = ack;
            this->_zero_window_size = window_size == 0;
            uint16_t new_window_size = this->_zero_window_size ? 1 : window_size;
            _last_window_size = static_cast<uint64_t>(
            std::max(0l, static_cast<int64_t>(ack) + new_window_size - static_cast<int64_t>(_next_seqno)));
        }
    } else if (ack <= _next_seqno) {
        new_data = true;
        _last_ackno = ack;
        this->_zero_window_size = window_size == 0;
        uint16_t new_window_size = this->_zero_window_size ? 1 : window_size;
        _last_window_size = static_cast<uint64_t>(
            std::max(0l, static_cast<int64_t>(ack) + new_window_size - static_cast<int64_t>(_next_seqno)));
    }
    if (!new_data) {
        return;
    }
    std::vector<std::pair<uint64_t, TCPSegment>> segments;
    for (auto kv : _outstanding_segs) {
        TCPSegment segment = kv.second;
        uint64_t seqno = kv.first;
        if (seqno + segment.length_in_sequence_space() > _last_ackno) {
            segments.emplace_back(seqno, segment);
        }
    }
    _outstanding_segs.clear();
    _bytes_in_flight = 0;
    for (const auto &kv : segments) {
        _outstanding_segs.insert(kv);
        _bytes_in_flight += kv.second.length_in_sequence_space();
    }

    _rto = _initial_retransmission_timeout;
    _consecutive_retransmissions = 0;
    if (_outstanding_segs.empty()) {
        _timer_ms = std::nullopt;
    } else {
        _timer_ms = _ms;
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _ms += ms_since_last_tick;
    if (!_timer_ms.has_value()) {
        return;
    }
    if (_ms - *_timer_ms >= _rto) {
        _timer_ms = _ms;
        auto seg = _outstanding_segs.begin()->second;
        _segments_out.push(seg);
        if (!_zero_window_size) {
            _consecutive_retransmissions += 1;
            _rto *= 2;
        }
    }
}

unsigned int TCPSender::consecutive_retransmissions() const {
    return _consecutive_retransmissions;
}

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    seg.header().seqno = wrap(_next_seqno, _isn);
    seg.header().syn = seg.header().fin = false;
    seg.payload() = Buffer("");
    _segments_out.push(seg);
}

uint64_t TCPSender::bytes_in_flight() const {
    return _bytes_in_flight;
}