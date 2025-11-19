//
// Created by lining on 10/21/24.
//

#include "FSM.h"
#include <cassert>
#include <cstring>

FSM::FSM(size_t capacity) {
  this->_capacity = capacity;
  this->buff = new uint8_t[capacity];
  this->_read_pos = 0;
  this->_write_pos = 0;

  this->_available_for_read = 0;
  this->_available_for_write = this->_capacity - this->_available_for_read;

  if (mtx == nullptr) {
    mtx = new std::mutex();
  }
}

FSM::~FSM() {
  delete[] buff;
  delete mtx;
}

size_t FSM::Read(void *data, size_t count) {
  assert(data != nullptr);

  //	printf("READ[%d]count=%ld,cap=%ld,read_pos=%d,write_pos=%d,ava_read=%d\n",
  //			__LINE__, count, rb->capacity, rb->read_pos,
  //rb->write_pos, 			rb->available_for_read);

  std::unique_lock<std::mutex> lock(*mtx);
  //	printf("READ[%d]count=%ld,cap=%ld,read_pos=%d,write_pos=%d,ava_read=%d\n",
  //			__LINE__, count, rb->capacity, rb->read_pos,
  //rb->write_pos, 			rb->available_for_read);
  if (_available_for_read >= count) {
    if (_read_pos + count > _capacity) {
      int len = _capacity - _read_pos;
      memcpy(data, buff + _read_pos, len);
      memcpy((uint8_t *)data + len, buff, count - len);
      _read_pos = count - len;
    } else {
      memcpy(data, buff + _read_pos, count);
      _read_pos += count;
    }

    _read_pos %= _capacity;
    _available_for_read -= count;
    _available_for_write = _capacity - _available_for_read;
  } else {
    printf("READ read error !\n");
    return 0;
  }

  //	printf("READ[%d]count=%ld,cap=%ld,read_pos=%d,write_pos=%d,ava_read=%d\n",
  //			__LINE__, count, rb->capacity, rb->read_pos,
  //rb->write_pos, 			rb->available_for_read);

  return count;
}

size_t FSM::Write(const void *data, size_t count) {
  assert(data != nullptr);
  //	printf("WRITE[%d]count=%ld,cap=%ld,read_pos=%d,write_pos=%d,ava_read=%d\n",
  //			__LINE__, count, rb->capacity, rb->read_pos,
  //rb->write_pos, 			rb->available_for_read);

  std::unique_lock<std::mutex> lock(*mtx);
  //	printf("WRITE[%d]count=%ld,cap=%ld,read_pos=%d,write_pos=%d,ava_read=%d\n",
  //			__LINE__, count, rb->capacity, rb->read_pos,
  //rb->write_pos, 			rb->available_for_read);
  if (_available_for_write >= count) {
    if (_write_pos + count > _capacity) {
      int len = _capacity - _write_pos;
      memcpy(&buff[_write_pos], data, len);
      memcpy(buff, (uint8_t *)data + len, count - len);
      _write_pos = count - len;
    } else {
      memcpy(&buff[_write_pos], data, count);
      _write_pos += count;
    }

    _write_pos %= _capacity;
    _available_for_read += count;
    _available_for_write = _capacity - _available_for_read;
  } else {
    printf("WRITE   error !\n");
    return 0;
  }
  //	printf("WRITE[%d]count=%ld,cap=%ld,read_pos=%d,write_pos=%d,ava_read=%d\n",
  //			__LINE__, count, rb->capacity, rb->read_pos,
  //rb->write_pos, 			rb->available_for_read);

  return count;
}

int FSM::GetReadLen() {
  std::unique_lock<std::mutex> lock(*mtx);
  int ret = _available_for_read;
  return ret;
}

int FSM::GetWriteLen() {
  std::unique_lock<std::mutex> lock(*mtx);
  int ret = _available_for_write;
  return ret;
}

void FSM::Stop() { _recv.wakeUpAll(); }

void FSM::TriggerAction(char *data, int len) {
  if (len > 0) {
    Write(data, len);
    _recv.enqueueNotification(
        Poco::AutoPtr<_RecvNotification>(new _RecvNotification(true)));
  }
}

bool FSM::WaitTriggerAction() {
  bool ret = false;
  Poco::AutoPtr<_RecvNotification> pNf =
      dynamic_cast<_RecvNotification *>(_recv.waitDequeueNotification());
  if (pNf) {
    ret = pNf->isRecv();
  }
  return ret;
}

bool FSM::ShouldAction() { return GetReadLen() >= _needLen; }
