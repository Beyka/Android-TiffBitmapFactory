package org.beyka.tiffbitmapfactory.exceptions;

/**
 * Created by alexeyba on 09.11.15.
 */
public class DecodeTiffException extends RuntimeException {

    private String fileName;
    private String aditionalInfo;

    public DecodeTiffException(String fileName){
        super("Could not decode tiff file " + fileName);
        this.fileName = fileName;
    }

    public DecodeTiffException(String fileName, String aditionaInfo){
        super("Could not decode tiff file " + fileName + "\n" + aditionaInfo);
        this.fileName = fileName;
        this.aditionalInfo = aditionaInfo;
    }

    public String getFileName(){
        return fileName;
    }

    public String getAditionalInfo() {
        return aditionalInfo;
    }
}
