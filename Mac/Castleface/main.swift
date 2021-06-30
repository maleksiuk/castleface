//
//  main.swift
//  Castleface
//
//  Created by Michael Aleksiuk on 6/26/21.
//

// Good places to learn from:
// https://theobendixson.medium.com/handmade-hero-osx-platform-layer-day-2-b26d6966e214
// https://github.com/TheoBendixson/Handmade-Hero-MacOS-Platform-Layer-Non-Video/blob/master/Day002/handmade/code/osx_main.mm
// https://github.com/itfrombit/osx_handmade



import Foundation
import AppKit

print("Hello, World!")

// TODO: this is duplicated in ppu.h
let videoBufferWidth = 256
let videoBufferHeight = 240

var running = true

let videoBufferSize = videoBufferWidth * videoBufferHeight * 4
var videoBuffer = [UInt8](repeating: 0, count: videoBufferSize)
videoBuffer.reserveCapacity(videoBufferSize)

// https://developer.apple.com/documentation/swift/unsafemutablepointer

let uint8Pointer = UnsafeMutablePointer<UInt8>.allocate(capacity: videoBufferSize)
uint8Pointer.initialize(from: &videoBuffer, count: videoBufferSize)

justForTesting(uint8Pointer)

//var computer = Computer.init()

//executeEmulatorCycle()

class MainWindowDelegate : NSObject, NSWindowDelegate {
    
    func windowWillClose(_ notification: Notification) {
        running = false
    }
}

let contentRect = NSRect.init(x: 0, y: 300, width: 300, height: 300)

let windowDelegate = MainWindowDelegate.init()

let window = NSWindow.init(contentRect: contentRect, styleMask: [.titled, .closable], backing: .buffered, defer: false)
window.contentView?.wantsLayer = true
window.backgroundColor = .red
window.makeKeyAndOrderFront(nil)
window.delegate = windowDelegate

while (running) {
    var event : NSEvent?
    
    repeat {
        event = NSApp.nextEvent(matching: .any, until: nil, inMode: .default, dequeue: true)
        
        if let event = event {
            debugPrint("got event")
            debugPrint(event)
            switch event.type {
            default:
                NSApp.sendEvent(event)
            }
        }
    } while (event != nil)
}





