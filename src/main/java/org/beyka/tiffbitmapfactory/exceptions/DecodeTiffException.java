package org.beyka.tiffbitmapfactory.exceptions;

/**
 * Created by alexeyba on 09.11.15.
 */
public class DecodeTiffException extends RuntimeException {

    private String fileName;
    private String aditionalInfo;
    private int fileDescriptor = -1;

    public DecodeTiffException(String fileName){
        super("Could not decode tiff file " + fileName);
        this.fileName = fileName;
    }

    public DecodeTiffException(int fileDescriptor) {
        super("Could not decode tiff file with file descriptor " + fileDescriptor);
        this.fileDescriptor = fileDescriptor;
    }

    public DecodeTiffException(String fileName, String aditionaInfo){
        super("Could not decode tiff file " + fileName + "\n" + aditionaInfo);
        this.fileName = fileName;
        this.aditionalInfo = aditionaInfo;
    }

    public DecodeTiffException(int fileDescriptor, String aditionaInfo){
        super("Could not decode tiff file with file descriptor" + fileDescriptor + "\n" + aditionaInfo);
        this.fileDescriptor = fileDescriptor;
        this.aditionalInfo = aditionaInfo;
    }

    public String getFileName(){
        return fileName;
    }

    public String getAditionalInfo() {
        return aditionalInfo;
    }

    public int getFileDescriptor() {
        return fileDescriptor;
    }
}
