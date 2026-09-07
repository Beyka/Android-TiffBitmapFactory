package org.beyka.tiffbitmapfactory.exceptions;

/**
 * Created by alexeyba on 09.11.15.
 */
public class NotEnoughtMemoryException extends RuntimeException {

    private final long availableMemory;
    private final long needMemory;

    public NotEnoughtMemoryException(long availableMemory, long needMemory){
        super("Available memory is not enought to decode image. Available " + availableMemory + " bytes. Need " + needMemory + " bytes.");
        this.availableMemory = availableMemory;
        this.needMemory = needMemory;
    }

    public long getAvailableMemory() {
        return availableMemory;
    }

    public long getNeedMemory() {
        return needMemory;
    }
}
