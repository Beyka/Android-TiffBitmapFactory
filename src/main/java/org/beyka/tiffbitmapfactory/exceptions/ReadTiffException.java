package org.beyka.tiffbitmapfactory.exceptions;

/**
 * Created by alexeyba on 09.11.15.
 */
public class ReadTiffException extends RuntimeException {

    private String fileName;

    public ReadTiffException(String fileName){
        super("Error while reading file " + fileName);
        this.fileName = fileName;
    }

    public String getFileName(){
        return fileName;
    }
}
