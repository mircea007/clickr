#pragma once

// X11
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <pthread.h>

#include "log.h"

const int DEFAULT_CPS = 20;

// event type lookup table
const int XEvent_types[33] = {
  KeyPress,       KeyRelease,       ButtonPress,     ButtonRelease,    MotionNotify,
  EnterNotify,    LeaveNotify,      FocusIn,         FocusOut,         KeymapNotify,
  Expose,         GraphicsExpose,   NoExpose,        CirculateRequest, ConfigureRequest,
  MapRequest,     ResizeRequest,    CirculateNotify, ConfigureNotify,  CreateNotify,
  DestroyNotify,  GravityNotify,    MapNotify,       MappingNotify,    ReparentNotify,
  UnmapNotify,    VisibilityNotify, ColormapNotify,  ClientMessage,    PropertyNotify,
  SelectionClear, SelectionNotify,  SelectionRequest
};

class SyncAutoClicker {
  protected:
    double CPS;
    int MIN_DELAY; // delays are in microseconds
    int MAX_DELAY;
    static const int RELEASE_WAIT = 100;
    static constexpr double REL_MIN = 0.5;
    static constexpr double REL_MAX = 1.5;
    Display *display;
    unsigned int btn;

    // https://gist.github.com/pioz/726474
    void click();

  public:
    SyncAutoClicker( unsigned int button, double cps );
    
    void setCPS( double cps );

    ~SyncAutoClicker();

    void autoclick( int num );
};


class AsyncAutoClicker : public SyncAutoClicker {
  protected:
    pthread_t worker_thread;

    // shared variables
    enum ClickerStatus { EXIT, CLICKING, WAITING } status_flag;
    pthread_mutex_t status_flag_mtx;

    //int MIN_DELAY, MAX_DELAY; // <--- already declared in parrent class
    pthread_mutex_t DELAY_mtx;
    
    static void *worker( void *args );

  public:
    AsyncAutoClicker( unsigned int button, double cps );
    
    ~AsyncAutoClicker();

    void setCPS( double cps );

    void start();

    void stop();
    
    ClickerStatus getStatus();
};