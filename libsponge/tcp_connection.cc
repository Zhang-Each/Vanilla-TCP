#include "tcp_connection.hh"

#include <iostream>
#include <optional>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const {
    return _sender.stream_in().remaining_capacity();
}

size_t TCPConnection::bytes_in_flight() const {
    return _sender.bytes_in_flight();
}

size_t TCPConnection::unassembled_bytes() const {
    return _receiver.unassembled_bytes();
}

size_t TCPConnection::time_since_last_segment_received() const { return {}; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    _segment_received_ms = _ms;
    if (seg.header().rst) {
        _is_active = false;
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
    } else {
        _receiver.segment_received(seg);
        if (seg.header().ack) {
            _sender.ack_received(seg.header().ackno, seg.header().win);
        }
        if (_fin_sent) {
            if (seg.header().ackno == _sender.next_seqno()) {
                _fin_sent_and_acked = true;
                if (!_linger_after_streams_finish) {
                    _is_active = false;
                }
            }
        }
        if (_receiver.stream_out().input_ended() && !_fin_sent) {
            _linger_after_streams_finish = false;
        }
        if (_receiver.ackno() != std::nullopt) {
            _sender.fill_window();
            if (_sender.segments_out().empty() && seg.length_in_sequence_space() >= 1) {
                _sender.send_empty_segment();
            }
            send_all_segments();
        }
    }
}

bool TCPConnection::active() const { 
    return _is_active;
}

size_t TCPConnection::write(const string &data) {
    size_t result = _sender.stream_in().write(data);
    _sender.fill_window();
    send_all_segments();
    return result;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _ms += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    if (time_since_last_segment_received() >= 10 * _cfg.rt_timeout) {
        if (_fin_sent_and_acked && _receiver.stream_out().input_ended()) {
            _is_active = false;
        }
    }
    if (_sender.consecutive_retransmissions() <= TCPConfig::MAX_RETX_ATTEMPTS) {
        send_all_segments();
    } else {
        send_reset();
    }
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    send_all_segments();
}

void TCPConnection::connect() {
    _sender.fill_window();
    send_all_segments();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            send_reset();
            // Your code here: need to send a RST segment to the peer
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::send_all_segments() {
    while (!_sender.segments_out().empty()) {
        send_segment(_sender.segments_out().front());
        _sender.segments_out().pop();
    }
}

void TCPConnection::send_segment(const TCPSegment &segment) {
    TCPSegment seg = segment;
    if (_receiver.ackno() != std::nullopt) {
        seg.header().ack = true;
        seg.header().ackno = *_receiver.ackno();
    }
    seg.header().win = _receiver.window_size();
    _fin_sent |= seg.header().fin;
    _segments_out.push(seg);
}

void TCPConnection::send_reset() {
    while (!_sender.segments_out().empty()) {
        _sender.segments_out().pop();
    }
    _sender.send_empty_segment();
    auto segment = _sender.segments_out().front();
    _sender.segments_out().pop();

    segment.header().rst = true;

    send_segment(segment);
    _is_active = false;
    _receiver.stream_out().set_error();
    _sender.stream_in().set_error();
}