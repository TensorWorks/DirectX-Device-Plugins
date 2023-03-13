//go:build windows

package plugin

import (
	"github.com/fsnotify/fsnotify"
)

type DeletionWatcher struct {
	watcher *fsnotify.Watcher
	Deleted chan struct{}
	Errors  chan error
}

func WatchForDeletion(file string) (*DeletionWatcher, error) {

	// Create a new filesystem watcher
	fsWatcher, err := fsnotify.NewWatcher()
	if err != nil {
		return nil, err
	}

	// Add a watch for the specified file
	err = fsWatcher.Add(file)
	if err != nil {
		return nil, err
	}

	// Wrap the filesystem watcher in a deletion watcher
	deletionWatcher := &DeletionWatcher{
		watcher: fsWatcher,
		Deleted: make(chan struct{}, 1),
		Errors:  make(chan error, 1),
	}

	// Start the watcher goroutine
	go deletionWatcher.watch()
	return deletionWatcher, nil
}

// Cancels the watch
func (d *DeletionWatcher) Cancel() {
	d.watcher.Close()
}

func (d *DeletionWatcher) watch() {

	// Ensure the underlying filesystem watcher is closed when we are done
	defer d.watcher.Close()

	// Ensure the channels are closed when we are done
	defer close(d.Deleted)
	defer close(d.Errors)

	// Process events and errors
	for {
		select {
		case event, ok := <-d.watcher.Events:
			if !ok {
				return
			}

			// Check whether the event is a deletion event
			if event.Op&fsnotify.Remove == fsnotify.Remove {
				d.Deleted <- struct{}{}
				return
			}

		case err, ok := <-d.watcher.Errors:
			if !ok {
				return
			}

			d.Errors <- err
		}
	}
}
