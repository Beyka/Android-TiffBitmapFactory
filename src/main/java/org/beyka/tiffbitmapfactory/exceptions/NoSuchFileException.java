package org.beyka.tiffbitmapfactory.exceptions;

/**
 * Created by alexeyba on 09.11.15.
 */
public class NoSuchFileException extends RuntimeException {
    private String fileName;

    public NoSuchFileException(String fileName){
        super("No such file " + fileName);
        this.fileName = fileName;
    }

    public String getFileName(){
        return fileName;
    }
}
