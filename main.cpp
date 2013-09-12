#include <unistd.h>
#include <cstdio>
#include "x.hpp"
#include "options.hpp"

int main( int argc, char** argv ) {
    int err = options->parseOptions( argc, argv );
    if ( err ) {
        return err;
    }
    int state = 0;
    bool running = true;
    slop::Rectangle* selection = NULL;
    slop::Rectangle* windowselection = NULL;
    Window window = None;
    std::string xdisplay = options->m_xdisplay;
    int padding = options->m_padding;
    int borderSize = options->m_borderSize;
    int tolerance = options->m_tolerance;
    float r = options->m_red;
    float g = options->m_green;
    float b = options->m_blue;
    timespec start, time;
    int cx = 0;
    int cy = 0;

    // First we set up the x interface and grab the mouse,
    // if we fail for either we exit immediately.
    err = xengine->init( xdisplay.c_str() );
    if ( err ) {
        return err;
    }
    err = xengine->grabCursor( slop::Cross );
    if ( err ) {
        return err;
    }
    err = xengine->grabKeyboard();
    if ( err ) {
        return err;
    }
    clock_gettime( CLOCK_REALTIME, &start );
    while ( running ) {
        clock_gettime( CLOCK_REALTIME, &time );
        // "ticking" the xengine makes it process all queued events.
        xengine->tick();
        // If the user presses any key on the keyboard, exit the application.
        // Make sure a second has passed before allowing canceling
        if ( time.tv_sec - start.tv_sec > 1 ) {
            if ( xengine->m_keypressed ) {
                printf( "X=0\n" );
                printf( "Y=0\n" );
                printf( "W=0\n" );
                printf( "H=0\n" );
                fprintf( stderr, "User pressed key. Canceled selection.\n" );
                state = -1;
                running = false;
            }
        } else {
            xengine->m_keypressed = false;
        }
        if ( xengine->mouseDown( 3 ) ) {
            printf( "X=0\n" );
            printf( "Y=0\n" );
            printf( "W=0\n" );
            printf( "H=0\n" );
            fprintf( stderr, "User right-clicked. Canceled selection.\n" );
            state = -1;
            running = false;
        }
        // Our adorable little state manager will handle what state we're in.
        switch ( state ) {
            default: {
                break;
            }
            case 0: {
                // If xengine has found a window we're hovering over (or if it changed)
                // create a rectangle around it so the user knows he/she can click on it.
                if ( window != xengine->m_hoverXWindow ) {
                    // Make sure to delete the old selection rectangle.
                    if ( windowselection ) {
                        xengine->removeRect( windowselection ); // removeRect also dealloc's the rectangle for us.
                    }
                    slop::WindowRectangle t = xengine->m_hoverWindow;
                    windowselection = new slop::Rectangle( t.m_x - t.m_border,
                                                         t.m_y - t.m_border,
                                                         t.m_width + t.m_border,
                                                         t.m_height + t.m_border,
                                                         borderSize, padding,
                                                         r, g, b );
                    xengine->addRect( windowselection );
                    window = xengine->m_hoverXWindow;
                }
                // If the user clicked, remove the old selection rectangle and then
                // move on to the next state.
                if ( xengine->mouseDown( 1 ) ) {
                    if ( windowselection ) {
                        xengine->removeRect( windowselection );
                    }
                    state++;
                }
                break;
            }
            case 1: {
                // Set the mouse position of where we clicked, used so that click tolerance doesn't affect the rectangle's position.
                cx = xengine->m_mousex;
                cy = xengine->m_mousey;
                state++;
                break;
            }
            case 2: {
                // If the user has let go of the mouse button, we'll just
                // continue to the next state.
                if ( !xengine->mouseDown( 1 ) ) {
                    state++;
                    break;
                }
                // Check to make sure the user actually wants to drag for a selection before creating a rectangle.
                int w = xengine->m_mousex - cx;
                int h = xengine->m_mousey - cy;
                if ( ( std::abs( w ) > tolerance || std::abs( h ) > tolerance ) && !selection ) {
                    selection = new slop::Rectangle( cx, cy, 0, 0, borderSize, padding, r, g, b );
                    xengine->addRect( selection );
                } else if ( std::abs( w ) <= tolerance && std::abs( h ) <= tolerance ) {
                    continue;
                }
                // Set the selection rectangle's dimensions to mouse movement.
                // We use the function setDim since rectangles can't have negative widths,
                // and because the rectangles have borders and padding to worry about.
                selection->setDim( w, h );
                // We also detect which way the user is pulling and set the mouse icon accordingly.
                bool x = selection->m_flippedx;
                bool y = selection->m_flippedy;
                if ( !x && !y ) {
                    xengine->setCursor( slop::LowerRightCorner );
                } else if ( x && !y ) {
                    xengine->setCursor( slop::LowerLeftCorner );
                } else if ( !x && y ) {
                    xengine->setCursor( slop::UpperRightCorner );
                } else {
                    xengine->setCursor( slop::UpperLeftCorner );
                }

                break;
            }
            case 3: {
                int x, y, w, h;
                // Exit the utility after this state runs once.
                running = false;
                if ( selection ) {
                    // We pull the dimensions and positions from the selection rectangle.
                    // The selection rectangle automatically converts the positions and
                    // dimensions to absolute coordinates when we set them earilier.
                    x = selection->m_x+selection->m_xoffset;
                    y = selection->m_y+selection->m_yoffset;
                    w = selection->m_width;
                    h = selection->m_height;
                    // Delete the rectangle.
                    xengine->removeRect( selection );
                    // If the user simply clicked (and thus made the width and height smaller than
                    // our tolerance) or if we're not hovering over a window, just print the selection
                    // rectangle's stuff.
                    if ( w > tolerance || h > tolerance || xengine->m_hoverXWindow == None ) {
                        printf( "X=%i\n", x );
                        printf( "Y=%i\n", y );
                        printf( "W=%i\n", w + 1 );
                        printf( "H=%i\n", h + 1 );
                        break;
                    }
                }
                // Otherwise lets grab the window's dimensions and use those (with padding).
                slop::WindowRectangle t = xengine->m_hoverWindow;
                x = t.m_x - padding - t.m_border;
                y = t.m_y - padding - t.m_border;
                w = t.m_width + t.m_border + padding*2;
                h = t.m_height + t.m_border + padding*2;
                printf( "X=%i\n", x );
                printf( "Y=%i\n", y );
                printf( "W=%i\n", w );
                printf( "H=%i\n", h );
                break;
            }
        }
        // No need to max out CPU
        // FIXME: This could be adjusted to measure how much time has passed,
        // we may very well need to max out the CPU if someone has a really- really
        // bad computer.
        usleep( 1000 );
    }
    xengine->releaseCursor();
    xengine->releaseKeyboard();
    // Clean up global classes.
    delete xengine;
    delete options;
    return 0;
}
