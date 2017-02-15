package org.beyka.tiffbitmapfactory.exceptions;

/**
 * Created by alexeyba on 09.11.15.
 */
public class NotEnoughtMemoryException extends RuntimeException {

    private int availableMemory;
    private int needMemory;

    public NotEnoughtMemoryException(int availableMemory, int needMemory){
        super("Available memory is not enought to decode image. Available " + availableMemory + " bytes. Need " + needMemory + " bytes.");
        this.availableMemory = availableMemory;
        this.needMemory = needMemory;
    }

    public int getAvailableMemory() {
        return availableMemory;
    }

    public int getNeedMemory() {
        return needMemory;
    }
}
