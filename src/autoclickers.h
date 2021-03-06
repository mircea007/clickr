#pragma once

#include "log.h"
#include "os_specific.h"

const int DEFAULT_CPS = 20;

#define LEFT_CLICK 0
#define MIDDLE_CLICK 1
#define RIGHT_CLICK 2

#ifdef OS_IS_UNIX // linux/freebsd

// X11
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <pthread.h>

class SyncAutoClicker {
  protected:
    double CPS;
    int MIN_DELAY; // delays are in microseconds
    int MAX_DELAY;
    static const int RELEASE_WAIT = 100;
    static constexpr double REL_MIN = 0.75;
    static constexpr double REL_MAX = 1.25;
    Display *display;
    unsigned int btn;

    // https://gist.github.com/pioz/726474
    void click();

  public:
    SyncAutoClicker( int button, double cps );
    
    void setCPS( double cps );

    ~SyncAutoClicker();

    void autoclick( int num );
};

#else // windows

#include <windows.h>

class SyncAutoClicker {
  protected:
    double CPS;
    int MIN_DELAY; // delays are in microseconds
    int MAX_DELAY;
    static const int RELEASE_WAIT = 100;
    static constexpr double REL_MIN = 0.75;
    static constexpr double REL_MAX = 1.25;
    int btn;
    DWORD buttons[3][2] = {
      { MOUSEEVENTF_LEFTDOWN,   MOUSEEVENTF_LEFTUP   },
      { MOUSEEVENTF_MIDDLEDOWN, MOUSEEVENTF_MIDDLEUP },
      { MOUSEEVENTF_RIGHTDOWN,  MOUSEEVENTF_RIGHTUP  },
    };

    void click();

  public:
    SyncAutoClicker( int button, double cps );
    
    void setCPS( double cps );

    ~SyncAutoClicker();

    void autoclick( int num );
};

#endif

// os-neutral

class AsyncAutoClicker : public SyncAutoClicker {
  protected:
    thread_t worker_thread;

    // shared variables
    enum ClickerStatus { EXIT, CLICKING, WAITING } status_flag;
    mutex_t status_flag_mtx;

    //int MIN_DELAY, MAX_DELAY; // <--- already declared in parrent class
    mutex_t DELAY_mtx;
    
    static thread_ret_type THREAD_FUNC_ATTR worker( LPVOID args );

  public:
    AsyncAutoClicker( int button, double cps );
    
    ~AsyncAutoClicker();

    void setCPS( double cps );

    void start();

    void stop();
    
    ClickerStatus getStatus();
};
