/*
 * RACK - Robotics Application Construction Kit
 * Copyright (C) 2005-2006 University of Hannover
 *                         Institute for Systems Engineering - RTS
 *                         Professor Bernardo Wagner
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Authors
 *      Marko Reimer     <reimer@l3s.de>
 *      Joerg Langenberg <joerg.langenberg@gmx.net>
 *
 */
#ifndef __IMAGE_TOOL_H__
#define __IMAGE_TOOL_H__

class ImageTool {
	public:

    static int clip(int in)
    {
        if (in < 0)
        {
            return 0;
        }
        else if (in > 255)
        {
            return 255;
        }
        return in;
    }

    static int convertCharUYVY2RGB(uint8_t* outputData, uint8_t* inputData,
                            int width, int height)
    {
        int i,j;
        int u,y0,v,y1;

        for (i = 0; i < height; i++)
        {
            for (j = 0; j < width; j += 2) //step = 2
            {//handling 4 byte <=> 2 pixel each turn
                u  = (int) (0xff & inputData[2 * (i * width + j) + 0]);
                y0 = (int) (0xff & inputData[2 * (i * width + j) + 1]);
                v  = (int) (0xff & inputData[2 * (i * width + j) + 2]);
                y1 = (int) (0xff & inputData[2 * (i * width + j) + 3]);

                outputData[3 * (i * width + j) + 2] = (uint8_t) clip(( 298 * (y0-16)                 + 409 * (v-128) + 128) >> 8);//bo
                outputData[3 * (i * width + j) + 1] = (uint8_t) clip(( 298 * (y0-16) - 100 * (u-128) - 208 * (v-128) + 128) >> 8);//g0
                outputData[3 * (i * width + j) + 0] = (uint8_t) clip(( 298 * (y0-16) + 516 * (u-128)                 + 128) >> 8);//r0

                outputData[3 * (i * width + j) + 5] = (uint8_t) clip(( 298 * (y1-16)                 + 409 * (v-128) + 128) >> 8);
                outputData[3 * (i * width + j) + 4] = (uint8_t) clip(( 298 * (y1-16) - 100 * (u-128) - 208 * (v-128) + 128) >> 8);
                outputData[3 * (i * width + j) + 3] = (uint8_t) clip(( 298 * (y1-16) + 516 * (u-128)                 + 128) >> 8);
            }
        }
        return 0;
    }


	static int convertCharUYVY2BGR(uint8_t* outputData, uint8_t* inputData,
	                               int width, int height)
	{
	    int i,j;
	    int u,y0,v,y1;

	    for (i = 0; i < height; i++)
	    {
	        for (j = 0; j < width; j += 2) //step = 2
	        {//handling 4 byte <=> 2 pixel each turn
	            u  = (int) (0xff & inputData[2 * (i * width + j) + 0]);
	            y0 = (int) (0xff & inputData[2 * (i * width + j) + 1]);
	            v  = (int) (0xff & inputData[2 * (i * width + j) + 2]);
	            y1 = (int) (0xff & inputData[2 * (i * width + j) + 3]);

	            outputData[3 * (i * width + j) + 0] = (uint8_t) clip(( 298 * (y0-16)                 + 409 * (v-128) + 128) >> 8);//bo
	            outputData[3 * (i * width + j) + 1] = (uint8_t) clip(( 298 * (y0-16) - 100 * (u-128) - 208 * (v-128) + 128) >> 8);//g0
	            outputData[3 * (i * width + j) + 2] = (uint8_t) clip(( 298 * (y0-16) + 516 * (u-128)                 + 128) >> 8);//r0

	            outputData[3 * (i * width + j) + 3] = (uint8_t) clip(( 298 * (y1-16)                 + 409 * (v-128) + 128) >> 8);
	            outputData[3 * (i * width + j) + 4] = (uint8_t) clip(( 298 * (y1-16) - 100 * (u-128) - 208 * (v-128) + 128) >> 8);
	            outputData[3 * (i * width + j) + 5] = (uint8_t) clip(( 298 * (y1-16) + 516 * (u-128)                 + 128) >> 8);
	        }
	    }
	    return 0;
	}


	static int convertCharUYVY2Gray(uint8_t* outputData, uint8_t* inputData,
	                                int width, int height)
	{
	    int i,j;

	    for (i = 0; i < height; i++)
	    {
	        for (j = 0; j < width; j++ )
	        {
	            outputData[i * width + j] = inputData[(2 * (i * width + j)) + 1];
	        }
	    }
	    return 0;
	}
	
		static int convertCharBGR2RGB(uint8_t* outputData, uint8_t* inputData,
	                                int width, int height)
	{
	    int i,j;
	    uint8_t swap;

	    for (i = 0; i < height; i++)
	    {
	        for (j = 0; j < width; j++ )
	        {
	            swap                                = inputData[(i * width + j) * 3];//swap = input_B
	            outputData[(i * width + j) * 3]     = inputData[(i * width + j) * 3 + 2];
	            outputData[(i * width + j) * 3 + 2] = swap;
	        }
	    }
	    return 0;
	}
};


#endif // __IMAGE_TOOL_H__
