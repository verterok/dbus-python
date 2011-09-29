import _dbus_py
import dbus
from twisted.internet.abstract import FileDescriptor
import threading

class DBusReader(FileDescriptor):

    def __init__(self, watch, loop, reactor=None):
        FileDescriptor.__init__(self)
        if reactor is None:
            from twisted.internet import reactor
        self.reactor = reactor
        self.watch = watch
        self.loop = loop

    def writeSomeData(self, data):
        print "writeSomeData"
        if self.watch.enabled:
            #self.loop.dispatch()
            self.watch.handle_watch()

    def doRead(self):
        if self.watch.enabled:
            print "doRead", self.watch, ' - ', threading.currentThread()
            print "watch.handle_watch"
            self.watch.handle_watch()
            print "dispatch"
            self.loop.dispatch()
            print "dispatch done!"
        else:
            print "doRead, watch disabled", self.watch

    def fileno(self):
        return self.watch.fd

class MyLoop(_dbus_py.BaseMainLoop):

    def __init__(self, set_as_default=False, reactor=None):
        super(MyLoop, self).__init__(set_as_default=set_as_default)
        if reactor is None:
            from twisted.internet import reactor
        self.reactor = reactor
        self.watches = {}
        self.timeouts = {}

    def add_watch(self, watch):
        print "*** python *** - add_watch:", watch
        self.watches[watch] = None
        if watch.enabled:
            reader = DBusReader(watch, self)
            self.watches[watch] = reader
            if watch.readable:
                self.reactor.addReader(reader)
            if watch.writable:
                self.reactor.addWriter(reader)

    def remove_watch(self, watch):
        print "*** python *** -  remove_watch:", watch, " - ", threading.currentThread()
        reader = self.watches.pop(watch, None)
        if reader is not None:
            if watch.readable:
                print "   removing reader"
                self.reactor.removeReader(reader)
            if watch.writable:
                print "   removing writer"
                self.reactor.removeWriter(reader)

    def toggle_watch(self, watch):
        print "*** python *** toggle_watch - watch:", watch

    def add_timeout(self, timeout):
        print "*** python *** add_timeout - ", timeout, " - ", threading.currentThread()
        self.timeouts[timeout] = None
        # TODO
        print "   add the callLater", timeout.interval
        delayed_call = self.reactor.callLater(timeout.interval,
                         timeout.handle_timeout)
        self.timeouts[timeout] = delayed_call

    def remove_timeout(self, timeout):
        print "*** python *** remove_timeout - ", timeout, " - ", threading.currentThread()
        delayed_call = self.timeouts.pop(timeout, None)
        print "   delayed_call", delayed_call
        if delayed_call is not None and delayed_call.active():
            print "   removing", delayed_call
            delayed_call.cancel()

    def toggle_timeout(self, timeout):
        print "*** python *** toggle_timeout -", timeout

    def wakeup_main(self):
        print "*** python *** - wakeup_main", ' - ', threading.currentThread()
        self.reactor.wakeUp()

import gc
gc.set_debug(gc.DEBUG_LEAK)

def main():
    l = MyLoop(set_as_default=True)
    s = dbus.SessionBus()
    #s.add_match_string("interface=com.ubuntuone.SyncDaemon")
    obj = s.get_object('com.ubuntuone.SyncDaemon', '/status', )
    print obj.current_status()
    def show_status(*a):
        gc.collect()
        print "---------- Status Changed:", a
    s.add_signal_receiver(show_status, signal_name='StatusChanged')
    print obj.current_status()
    #s.close()

from twisted.internet import reactor
reactor.callWhenRunning(main)
reactor.run()
