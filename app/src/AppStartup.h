#pragma once

class QQuickWindow;

/**
 * How long the app took to come up, and where the time went.
 *
 * One number is not enough to act on. "Six hundred milliseconds" says nothing about whether to look
 * at the dictionary, the QML or the loader, so each stage is timed separately and reported as a
 * breakdown.
 *
 * Two numbers matter and they are not the same. QML ready is when the engine has built the object
 * tree; the first frame is when something is actually on screen, which is later by however long the
 * scene graph takes to render and the compositor to show it. The second is what the user
 * experiences, and it cannot be measured from inside the code that causes it - only by waiting for
 * the frame to be swapped.
 */
namespace AppStartup
{
    /**
     * Starts the clock. The first statement of main, before the application object, since
     * constructing that is itself one of the stages worth seeing.
     *
     * On Windows the time before this point is recoverable too: the process creation time says how
     * long the loader spent mapping Qt and the speech runtime before any of this code ran, which on
     * a debug build is a real share of the total and is invisible from anywhere else.
     */
    void Begin();

    /**
     * Records the time since the previous mark under this name. Marks are held rather than logged,
     * because the earliest of them happen before the log file exists.
     *
     * Name is a literal, kept by pointer.
     */
    void Mark(const char* Stage);

    /** Writes the breakdown so far. Called once the object tree exists, so a start that never shows
        a window still reports something. */
    void ReportReady();

    /** Logs the total once a frame has actually reached the screen, then disconnects. Does nothing
        for a window that is never shown. */
    void ReportFirstFrame(QQuickWindow* Window);
}
