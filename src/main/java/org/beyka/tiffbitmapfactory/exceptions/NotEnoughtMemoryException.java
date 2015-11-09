package org.beyka.tiffbitmapfactory.exceptions;

/**
 * Created by alexeyba on 09.11.15.
 */
public class NotEnoughtMemoryException extends RuntimeException {

    private int allocatedMemory;

    public NotEnoughtMemoryException(int memory){
        super("Allocated memory is not enought to decode image. Allocated " + memory + " bytes");
        this.allocatedMemory = memory;
    }

    public int getAllocatedMemory(){
        return allocatedMemory;
    }

}
