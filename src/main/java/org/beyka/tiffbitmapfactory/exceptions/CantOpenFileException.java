package org.beyka.tiffbitmapfactory.exceptions;

/**
 * Created by alexeyba on 09.11.15.
 */
public class CantOpenFileException extends RuntimeException {
    private String fileName;
    private int fileDescriptor = -1;

    public CantOpenFileException(String fileName){
        super("Can\'t open file " + fileName);
        this.fileName = fileName;
    }

    public CantOpenFileException(int fileDescriptor){
        super("Can\'t open file with file descriptor " + fileDescriptor);
        this.fileDescriptor = fileDescriptor;
    }

    public String getFileName(){
        return fileName;
    }

    public int getFileDescriptor() {
        return fileDescriptor;
    }
}
