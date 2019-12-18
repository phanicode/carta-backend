#ifndef __SESSION_CONTEXT_H__
#define __SESSION_CONTEXT_H__

class SessionContext {
 public:
  SessionContext() {
    _cancelled = false;
  }
  void cancel_group_execution() {
    _cancelled = true;
  }
  bool is_group_execution_cancelled() {
    return _cancelled;
  }
  void reset() {
    _cancelled = false;
  }
  
 private:
  mutable bool _cancelled;
};

#endif // __SESSION_CONTEXT_H__
