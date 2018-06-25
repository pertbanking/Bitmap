#include "bitmap.h"

#include <iostream>
#include <fstream>
#include <cstdlib>
#include <cstdint>

typedef unsigned char uchar_t;

const int MIN_RGB=0;
const int MAX_RGB=255;
const int BMP_MAGIC_ID=2;


/// Windows BMP-specific format data
struct bmpfile_magic
{
    uchar_t magic[BMP_MAGIC_ID];
};

/**
 * Generic 14-byte bitmap header
 */
struct bmpfile_header
{
    uint32_t file_size;  ///< The number of bytes in the bitmap file.
    uint16_t creator1;   ///< Two bytes reserved.
    uint16_t creator2;   ///< Two bytes reserved.
    uint32_t bmp_offset; ///< Offset from beginning to bitmap bits.
};

/**
 * @brief Mircosoft's defined header structure for Bitmap version 3.x.
 * 
 * https://msdn.microsoft.com/en-us/library/dd183376%28v=vs.85%29.aspx
 */
struct bmpfile_dib_info
{
  uint32_t header_size;           ///< The size of this header.
  int32_t  width;
  int32_t  height;
  uint16_t num_planes;            ///< Number of planes. Almost always 1.
  uint16_t bits_per_pixel;        ///< Bits per pixel. Can be 0, 1, 4, 8, 16, 24, or 32.
  uint32_t compression;           ///< https://msdn.microsoft.com/en-us/library/cc250415.aspx
  uint32_t bmp_byte_size;         ///< The size of the image in bytes.
  int32_t  hres;
  int32_t  vres;
  uint32_t num_colors;            ///< The number of color indices used in the color table.
  uint32_t num_important_colors;  ///< The number of colors used by the bitmap.
};


/**
 * @brief Opens a file as its name is provided and reads pixel-by-pixel the colors
 * into a matrix of RGB pixels. 
 * 
 * Any errors will be echo'd to cout but will result in an empty matrix (with no 
 * rows and no columns).
 *
 * @param name of the filename to be opened and read as a matrix of pixels
**/
void Bitmap::open(std::string filename)
{
    std::ifstream file(filename.c_str(), std::ios::in | std::ios::binary);
    
    if (file.fail())
    {
        std::cout << filename << " could not be opened. Does it exist? "
                  << "Is it already open by another program?\n";
        //pixels.resize(0); //make empty if it isn't already
    }
    else
    {
        bmpfile_magic magic;
        file.read((char*)(&magic), sizeof(magic));
        
        // Check to make sure that the first two bytes of the file are the "BM"
        // identifier that identifies a bitmap image.
        if (magic.magic[0] != 'B' || magic.magic[1] != 'M')
        {
            std::cout << filename << " is not in proper BMP format; it does "
                                  << "not begin with the magic bytes!\n";
        }
        else
        {
            // clear data if already holds information
            for(int i = 0; i < pixels.size(); ++i)
            {
                pixels[i].clear();
            }
            pixels.clear();

            bmpfile_header header;
            file.read((char*)(&header), sizeof(header));

            bmpfile_dib_info dib_info;
            file.read((char*)(&dib_info), sizeof(dib_info));

            // Check for this here and so that we know later whether we need to insert
            // each row at the bottom or top of the image.
            bool flip = true;
            if (dib_info.height < 0)
            {
                flip = false;
                dib_info.height = -dib_info.height;
            }

            // Only support for 24-bit images
            if (dib_info.bits_per_pixel != 24)
            {
                std::cout<<filename<<" uses "<<dib_info.bits_per_pixel
                         <<"bits per pixel (bit depth). Bitmap only supports 24bit.\n";
            }

            // No support for compressed images
            if (dib_info.compression != 0)
            {
                std::cout<<filename<<" is compressed. "
                         <<"Bitmap only supports uncompressed images.\n";
            }

            file.seekg(header.bmp_offset);

            // Read the pixels for each row and column of Pixels in the image.
            for (int row = 0; row < dib_info.height; row++)
            {
                std::vector <Pixel> row_data;

                for (int col = 0; col < dib_info.width; col++)
                {
                    int blue = file.get();
                    int green = file.get();
                    int red = file.get();

                    row_data.push_back( Pixel(red, green, blue) );
                }

                // Rows are padded so that they're always a multiple of 4
                // bytes. This line skips the padding at the end of each row.
                file.seekg(dib_info.width % 4, std::ios::cur);

                if (flip)
                {
                    pixels.insert(pixels.begin(), row_data);
                }
                else
                {
                    pixels.push_back(row_data);
                }
            }

            file.close();
        }//end else (is an image)
    }//end else (can open file)
}

// ----------------------------------------------------------------------------
/**
 * Saves the current image, represented by the matrix of pixels, as a
 * Windows BMP file with the name provided by the parameter. File extension
 * is not forced but should be .bmp. Any errors will cout and will NOT 
 * attempt to save the file.
 *
 * @param name of the filename to be written as a bmp image
**/
void Bitmap::save(std::string filename)
{
    std::ofstream file(filename.c_str(), std::ios::out | std::ios::binary);

    if (file.fail())
    {
        std::cout<<filename<<" could not be opened for editing. "
                 <<"Is it already open by another program or is it read-only?\n";
        
    }
    else if( !isImage() )
    {
        std::cout<<"Bitmap cannot be saved. It is not a valid image.\n";
    }
    else
    {
        // Write all the header information that the BMP file format requires.
        bmpfile_magic magic;
        magic.magic[0] = 'B';
        magic.magic[1] = 'M';
        file.write((char*)(&magic), sizeof(magic));
        bmpfile_header header = { 0 };
        header.bmp_offset = sizeof(bmpfile_magic)
                + sizeof(bmpfile_header) + sizeof(bmpfile_dib_info);
        header.file_size = header.bmp_offset
                + (pixels.size() * 3 + pixels[0].size() % 4) * pixels.size();
        file.write((char*)(&header), sizeof(header));
        bmpfile_dib_info dib_info = { 0 };
        dib_info.header_size = sizeof(bmpfile_dib_info);
        dib_info.width = pixels[0].size();
        dib_info.height = pixels.size();
        dib_info.num_planes = 1;
        dib_info.bits_per_pixel = 24;
        dib_info.compression = 0;
        dib_info.bmp_byte_size = 0;
        dib_info.hres = 2835;
        dib_info.vres = 2835;
        dib_info.num_colors = 0;
        dib_info.num_important_colors = 0;
        file.write((char*)(&dib_info), sizeof(dib_info));

        // Write each row and column of Pixels into the image file -- we write
        // the rows upside-down to satisfy the easiest BMP format.
        for (int row = pixels.size() - 1; row >= 0; row--)
        {
            const std::vector <Pixel> & row_data = pixels[row];

            for (int col = 0; col < row_data.size(); col++)
            {
                const Pixel& pix = row_data[col];

                file.put((uchar_t)(pix.blue));
                file.put((uchar_t)(pix.green));
                file.put((uchar_t)(pix.red));
            }

            // Rows are padded so that they're always a multiple of 4
            // bytes. This line skips the padding at the end of each row.
            for (int i = 0; i < row_data.size() % 4; i++)
            {
                file.put(0);
            }
        }

        file.close();
    }
}
    
// ----------------------------------------------------------------------------
/**
  * Validates whether or not the current matrix of pixels represents a
  * proper image with non-zero-size rows and consistent non-zero-size
  * columns for each row. In addition, each pixel in the matrix is validated
  * to have red, green, and blue components with values between 0 and 255
  *
  * @return boolean value of whether or not the matrix is a valid image
 **/
bool Bitmap::isImage()
{
    const int height = pixels.size();

    if( height == 0 || pixels[0].size() == 0)
    {
        return false;
    }

    const int width = pixels[0].size();

    for(int row=0; row < height; row++)
    {
        if( pixels[row].size() != width )
        {
            return false;
        }
        for(int column=0; column < width; column++)
        {
            Pixel current = pixels[row][column];
            if( current.red > MAX_RGB || current.red < MIN_RGB ||
                  current.green > MAX_RGB || current.green < MIN_RGB ||
                  current.blue > MAX_RGB || current.blue < MIN_RGB )
                return false;
        }
    }
    return true;
}

// ----------------------------------------------------------------------------
/**
 * Provides a vector of vector of pixels representing the bitmap
 *
 * @return the bitmap image, represented by a matrix of RGB pixels
**/
PixelMatrix Bitmap::toPixelMatrix()
{
    if( isImage() )
    {
        return pixels;
    }   
    else
    {
        return PixelMatrix();
    }   
}

// ----------------------------------------------------------------------------
/**
 * Overwrites the current bitmap with that represented by a matrix of
 * pixels. Does not validate that the new matrix of pixels is a proper
 * image.
 *
 * @param a matrix of pixels to represent a bitmap
**/
void Bitmap::fromPixelMatrix(const PixelMatrix & values)
{
    pixels = values;
}
