#include "byte_stream.hh"
#include <iostream>
// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t cap) :
    buffer(), capacity(cap), is_end_input(false), num_read(0), num_write(0) {
}

size_t ByteStream::write(const string &data) {
    size_t len = min(remaining_capacity(), data.length());
    for (size_t i = 0; i < len; i ++) {
        buffer.push_back(data[i]);
    }
    num_write += len;
    return len;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    std::string result = "";
    for (size_t i = 0; i < len; i ++) {
        result += buffer.at(i);
    }
    return result;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    for (size_t i = 0; i < len; i ++) {
        buffer.pop_front();
    }
    num_read += len;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string res = peek_output(len);
    pop_output(len);
    return res;
}

void ByteStream::end_input() {
    is_end_input = true;
}

bool ByteStream::input_ended() const { 
    return is_end_input;
}

size_t ByteStream::buffer_size() const {
    return buffer.size();
}

bool ByteStream::buffer_empty() const { 
    return buffer.empty();
}

// 要输入结束了并且buffer读完了才能到EOF状态
bool ByteStream::eof() const { 
    return is_end_input && buffer_empty();
}

size_t ByteStream::bytes_written() const {
    return num_write;
}

size_t ByteStream::bytes_read() const {
    return num_read;
}

size_t ByteStream::remaining_capacity() const {
    return capacity - buffer.size();
}
