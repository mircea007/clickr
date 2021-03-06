#include "mimic.h"
#include "log.h"
#include <unistd.h> // usleep()

#ifdef OS_IS_UNIX // linux

#include <fcntl.h>  // to read form mouse device file
#include <stdlib.h> // system()
#include <stdio.h>  // popen() / pclose()
#include <string.h> // strcmp()

void *MimicMouseButFaster::worker( void *args ){
  MimicMouseButFaster *obj = (MimicMouseButFaster *)args;
  int bytes, left = 0, right = 0, newleft, newright;
  unsigned char data[3];
  
  log_info( "mouse listener initialized\n" );

  while( 1 ){// thread will exit when killed by destructor
    bytes = read( obj->fd, data, sizeof( data ) );
    
    pthread_mutex_lock( &obj->is_active_mtx );
    if( bytes > 0 && obj->is_active ){
      newleft = !!(data[0] & 0x1);
      newright = !!(data[0] & 0x2);
      
      switch( left + 2 * newleft ){
        case 2:
          obj->clickers[0]->start();
          break;
        case 1:
          obj->clickers[0]->stop();
          break;
      }
      
      switch( right + 2 * newright ){
        case 2:
          obj->clickers[1]->start();
          break;
        case 1:
          obj->clickers[1]->stop();
          break;
      }
      
      left = newleft;
      right = newright;
    }
    pthread_mutex_unlock( &obj->is_active_mtx );
  }

  return NULL;
}

void *MimicMouseButFaster::listen( void *args ){
  MimicMouseButFaster *obj = (MimicMouseButFaster *)args;
  XEvent event;
  unsigned int hotkey = XKeysymToKeycode( obj->display, XK_Caps_Lock );
  
  log_info( "hotkey listener initialized\n" );
  
  pthread_mutex_lock( &obj->status_flag_mtx );
  while( obj->status_flag != EXIT ){
    pthread_mutex_unlock( &obj->status_flag_mtx );
    
    XNextEvent( obj->display, &event );
    
    log_info( "event\n" );

    pthread_mutex_lock( &obj->is_active_mtx );
    log_info( "ok\n" );
    switch( event.type ){
      case DestroyNotify:
        log_info( "ok1\n" );
        //warn( "window destroy!\n" );
        XGetInputFocus( obj->display, &obj->curFocus, &obj->revert );
        XSelectInput( obj->display, obj->curFocus, LISTEN_MASK );
        break;
      case FocusOut:
        log_info( "ok2\n" );
        if( obj->curFocus != obj->root )
          XSelectInput( obj->display, obj->curFocus, 0 );
        XGetInputFocus( obj->display, &obj->curFocus, &obj->revert );
        if( obj->curFocus == PointerRoot )
          obj->curFocus = obj->root;
        XSelectInput( obj->display, obj->curFocus, LISTEN_MASK );

        break;
      case KeyPress:
        log_info( "ok3\n" );
        if( event.xkey.keycode == hotkey ){
          obj->is_active ^= 1;
          log_debug( "autoclick turned %s\n", obj->is_active ? "on" : "off");
        }
        break;
      /*default:
        //log_warn( "unknown event: searching in lookup table...\n" );
        int i = -1;
        while( (++i) < 33 && XEvent_types[i] != event.type );
        
        if( i >= 33 )
          log_warn( "uncaught event %d\n", event.type );
        else
          log_warn( "event is at index %d\n", i );
        break;*/
    }
    pthread_mutex_unlock( &obj->is_active_mtx );

    pthread_mutex_lock( &obj->status_flag_mtx );
  }

  return NULL;
}

MimicMouseButFaster::MimicMouseButFaster( double cps = DEFAULT_CPS ){
  clickers[0] = new AsyncAutoClicker( LEFT_CLICK, cps );
  clickers[1] = new AsyncAutoClicker( RIGHT_CLICK, cps );
  
  const char *caps_lock_cmd = "xset -q | grep Caps | awk '{print $4}'";
  char output[10];
  FILE *pipe = popen( caps_lock_cmd, "r" );
  if( !pipe )
    log_error( "Could not read initial Caps_Lock state\n" );
  
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wunused-result"// gcc magic to ingore usless warning

  fgets( output, 10, pipe );
  
  #pragma GCC diagnostic pop

  pclose( pipe );

  if( !strcmp( output, "on\n" ) ){
    is_active = 1;
  }else if( !strcmp( output, "off\n" ) ){
    is_active = 0;
  }else
    log_error( "Caps_Lock command gave malformed output -- fix on github: https://github.com/mircea007/autoclicker\n" );

  fd = open( pDevice, O_RDWR );
  
  if( fd == -1 )
    log_error( "Coudn't open %s\n  -- make sure you are running with root permision or that you hava acces to the device file\n", pDevice );
  
  status_flag = NORMAL;
  if( pthread_mutex_init( &status_flag_mtx, NULL ) )
    log_error( "Unable to create status_flag mutex\n" );

  if( pthread_mutex_init( &is_active_mtx, NULL ) )
    log_error( "Unable to create is_active mutex\n" );

  display = XOpenDisplay( NULL );
  if( !display )
    log_error( "Can't open display!\n" );

  root = DefaultRootWindow( display );
  
  XSetErrorHandler( catcher );

  XGetInputFocus( display, &curFocus, &revert );
  XSelectInput( display, curFocus, LISTEN_MASK );

  pthread_create( &worker_thread, NULL, worker, (void *)this );

  pthread_create( &listen_thread, NULL, listen, (void *)this );
}

MimicMouseButFaster::~MimicMouseButFaster(){
  pthread_cancel( worker_thread );// kill worker thread
  // this works because killing a thread in a read() call
  // is suposedly ok (acording to stackoverflow)
  
  pthread_mutex_lock( &status_flag_mtx );
  status_flag = EXIT;
  pthread_mutex_unlock( &status_flag_mtx );
  
  // to exit the hotkey listen thread we will send
  // a key press event to make XNextEvent() finish

  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wunused-result"// gcc magic to ingore usless warning
  
  system( "xdotool key Caps_Lock && xdotool key Caps_Lock" );

  #pragma GCC diagnostic pop
  
  pthread_mutex_destroy( &is_active_mtx );
  pthread_mutex_destroy( &status_flag_mtx );
  
  close( fd );
  
  XSetErrorHandler( NULL );

  XCloseDisplay( display );

  delete clickers[0];
  delete clickers[1];
}

int MimicMouseButFaster::catcher( Display *display, XErrorEvent *err ){
  if( err->error_code == BadWindow )
    log_warn( "BadWindow error -- leaving alone\n" );
  else
    log_error( "XErrorEvent of type %d\n", err->type );

  return 0;
}

#else // windows

#include <windows.h> // winapi

MimicMouseButFaster *MimicMouseButFaster::instance = NULL;// static instance pointer

const int STATUS_CHECK_DELAY = 50; // make smaller for faster checks (beware this will increase CPU usage)
const int KEY_STATE_MASK = 0x8000;
const int NUM_MOUSE_BTN = 3;
const int button_keys[3] = { VK_LBUTTON, VK_MBUTTON, VK_RBUTTON };

thread_ret_type MimicMouseButFaster::worker( LPVOID args ){
  MimicMouseButFaster *obj = (MimicMouseButFaster *)args;
  int is_active = 0;
  int mouse_states[3] = { 0, 0, 0 };
  int new_state;
  int i;
  
  lock_mutex( obj->status_flag_mtx );
  while( obj->status_flag != EXIT ){
    unlock_mutex( obj->status_flag_mtx );
    
    is_active ^= !!((GetAsyncKeyState( VK_CAPITAL ) & 0x0001));
    log_debug( "is_active = %d\n", is_active );
    
    for( i = 0 ; i < NUM_MOUSE_BTN ; i++ ){
      new_state = !!(GetAsyncKeyState( button_keys[i] ) & KEY_STATE_MASK);
      
      if( obj->clickers[i] && is_active ){
        switch( new_state + 2 * mouse_states[i] ){
          case 2:
            obj->clickers[i]->stop();
            break;
          case 1:
            obj->clickers[i]->start();
            break;
        }
      }
      
      mouse_states[i] = new_state;
    }
    
    usleep( STATUS_CHECK_DELAY );
    
    lock_mutex( obj->status_flag_mtx );
  }
  
  return (thread_ret_type)0;
}

MimicMouseButFaster::MimicMouseButFaster( double cps ){
  if( instance ){
    delete instance;
    instance = this;
  }

  clickers[0] = new AsyncAutoClicker( LEFT_CLICK, cps );
  clickers[1] = NULL;
  clickers[2] = new AsyncAutoClicker( RIGHT_CLICK, cps );
  
  status_flag = NORMAL;
  if( create_mutex( status_flag_mtx ) )
    log_error( "Unable to create status_flag mutex\n" );
  
  create_thread( worker_thread, worker, this );
}

MimicMouseButFaster::~MimicMouseButFaster(){
  lock_mutex( status_flag_mtx );
  status_flag = EXIT;
  unlock_mutex( status_flag_mtx );

  join_thread( worker_thread );
  
  delete clickers[0];
  delete clickers[1];

  instance = NULL;
}

#endif