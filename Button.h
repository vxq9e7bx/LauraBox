
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
struct Button;
std::vector<Button*> buttonList;

struct Button {
    explicit Button(int pin)
    : _pin(pin) {
      buttonList.push_back(this);
    }

    void setup() {
      pinMode(_pin, INPUT_PULLUP);
    }

    bool isCommandPending() {
      portENTER_CRITICAL_ISR(&timerMux);
      bool ret = _commandPending;
      _commandPending = false;
      portEXIT_CRITICAL_ISR(&timerMux);
      return ret;
    }

    void onTimer() {
      if(digitalRead(_pin) == HIGH) {
        // button not pressed
        _pressed = false;
        return;
      }

      if(_pressed) {
        ++_repeatCount;
        if(_repeatCount < 50) return;
      }
      _repeatCount = 0;
      _pressed = true;
    
      portENTER_CRITICAL_ISR(&timerMux);
      _commandPending = true;
      portEXIT_CRITICAL_ISR(&timerMux);
    }

  private:
    int _pin;
    bool _commandPending{false};
    bool _pressed{false};
    size_t _repeatCount{0};
};

