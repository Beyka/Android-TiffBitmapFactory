package org.beyka.tiffbitmapfactory.exceptions;

/**
 * Created by alexeyba on 09.11.15.
 */
public class DecodeTiffException extends RuntimeException {

    private String fileName;

    public DecodeTiffException(String fileName){
        super("Could not decode tiff file " + fileName);
        this.fileName = fileName;
    }

    public String getFileName(){
        return fileName;
    }
}
