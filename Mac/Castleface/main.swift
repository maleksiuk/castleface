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
// https://github.com/tarouszars/handmadehero_mac/blob/master/code/handmadehero.mac.mm



import Foundation
import AppKit

print("Hello, World!")

// TODO: this is duplicated in ppu.h
let videoBufferWidth = 256
let videoBufferHeight = 240

var running = true

let videoBufferSizeInBytes = videoBufferWidth * videoBufferHeight * 4

// https://developer.apple.com/documentation/swift/unsafemutablepointer

var videoBuffer : UnsafeMutablePointer<UInt8> = UnsafeMutablePointer<UInt8>.allocate(capacity: videoBufferSizeInBytes)
defer {
    videoBuffer.deallocate()
}

justForTesting(videoBuffer)

//var computer = Computer.init()

//executeEmulatorCycle()

class MainWindowDelegate : NSObject, NSWindowDelegate {
    
    func windowWillClose(_ notification: Notification) {
        running = false
    }
}

let contentRect = NSRect.init(x: 0, y: 300, width: videoBufferWidth, height: videoBufferHeight)

let windowDelegate = MainWindowDelegate.init()

let window = NSWindow.init(contentRect: contentRect, styleMask: [.titled, .closable], backing: .buffered, defer: false)
window.contentView?.wantsLayer = true
window.backgroundColor = .black
window.makeKeyAndOrderFront(nil)
window.delegate = windowDelegate

var event : NSEvent?

while (running) {
    
    videoBuffer.withMemoryRebound(to: UInt32.self, capacity: videoBufferSizeInBytes / 4) { videoBufferPixels in
        for y in 0..<videoBufferHeight {
            for x in 0..<videoBufferWidth {
                videoBufferPixels[y * videoBufferWidth + x] = 0xFF00FF00  // Alpha BB GG RR
            }
        }
    }
    
    var optionalPointerToVideoBuffer : UnsafeMutablePointer<UInt8>? = videoBuffer as UnsafeMutablePointer<UInt8>?
    //var optionalPointerToVideoBuffer = UnsafeMutablePointer?(videoBuffer)

//    debugPrint(optionalPointerToVideoBuffer?[0])
//    debugPrint(optionalPointerToVideoBuffer?[1])
//    debugPrint(optionalPointerToVideoBuffer?[2])
//    debugPrint(optionalPointerToVideoBuffer?[3])
    
    let imageRep = NSBitmapImageRep(bitmapDataPlanes: &optionalPointerToVideoBuffer, pixelsWide: videoBufferWidth, pixelsHigh: videoBufferHeight, bitsPerSample: 8, samplesPerPixel: 4, hasAlpha: true, isPlanar: false, colorSpaceName: .deviceRGB, bytesPerRow: 4*videoBufferWidth, bitsPerPixel: 32)
    
    let image = NSImage(size: NSMakeSize(CGFloat(videoBufferWidth), CGFloat(videoBufferHeight)))
    image.addRepresentation(imageRep!)
    window.contentView!.layer!.contents = image
    
    repeat {
        event = NSApp.nextEvent(matching: .any, until: nil, inMode: .default, dequeue: true)
        
        if let event = event {
//            debugPrint("got event")
//            debugPrint(event)
            switch event.type {
            default:
                NSApp.sendEvent(event)
            }
        }
    } while (event != nil)
}





