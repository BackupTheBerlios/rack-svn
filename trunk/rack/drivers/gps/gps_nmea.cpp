/*
 * RACK - Robotics Application Construction Kit
 * Copyright (C) 2005-2006 University of Hannover
 *                         Institute for Systems Engineering - RTS
 *                         Professor Bernardo Wagner
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * Authors
 *      Matthias Hentschel <hentschel@rts.uni-hannover.de>
 *
 */
#include <iostream>

#include "gps_nmea.h"

// init_flags (for init and cleanup)
#define INIT_BIT_DATA_MODULE                0
#define INIT_BIT_SERIALPORT_OPEN            1
#define INIT_BIT_MBX_WORK                   2
#define INIT_BIT_PROXY_POSITION             3

//
// data structures
//

GpsNmea *p_inst;

argTable_t argTab[] = {

    { ARGOPT_REQ, "serialDev", ARGOPT_REQVAL, ARGOPT_VAL_INT,
      "Serial device number", { -1 } },

    { ARGOPT_OPT, "positionInst", ARGOPT_REQVAL, ARGOPT_VAL_INT,
      "The instance number of the position module", { 0 } },

    { ARGOPT_OPT, "baudrate", ARGOPT_REQVAL, ARGOPT_VAL_INT,
      "Baudrate of serial device, default 4800", { 4800 } },

    { ARGOPT_OPT, "periodTime", ARGOPT_REQVAL, ARGOPT_VAL_INT,
      "PeriodTime of the GPS - Receiver (in ms), default 1000", { 1000 } },

    { ARGOPT_OPT, "trigMsg", ARGOPT_REQVAL, ARGOPT_VAL_INT,
      "NMEA-message to trigger (RMC = 0, GGA = 1, GSA = 2), default RMC (0)",
      { 0 } },

    { ARGOPT_OPT, "varXY", ARGOPT_REQVAL, ARGOPT_VAL_INT,
      "variance of xy position in mm [default 20000 mm]",
      { 20000 } },

    { ARGOPT_OPT, "varRho", ARGOPT_REQVAL, ARGOPT_VAL_INT,
      "variance of heading in deg [default 90 deg]",
      { 90 } },

  { 0, "", 0, 0, "", { 0 } } // last entry
};


struct rtser_config gps_serial_config = {
    config_mask       : 0xFFFF,
    baud_rate         : 0,
    parity            : RTSER_NO_PARITY,
    data_bits         : RTSER_8_BITS,
    stop_bits         : RTSER_1_STOPB,
    handshake         : RTSER_DEF_HAND,
    fifo_depth        : RTSER_DEF_FIFO_DEPTH,
    rx_timeout        : 200000000llu,
    tx_timeout        : RTSER_DEF_TIMEOUT,
    event_timeout     : RTSER_DEF_TIMEOUT,
    timestamp_history : RTSER_RX_TIMESTAMP_HISTORY,
    event_mask        : RTSER_EVENT_RXPEND
};

/*******************************************************************************
 *   !!! REALTIME CONTEXT !!!
 *
 *   moduleOn,
 *   moduleOff,
 *   moduleLoop,
 *   moduleCommand,
 *
 *   own realtime user functions
 ******************************************************************************/

int GpsNmea::moduleOn(void)
{
    serialPort.clean();
    // set rx timeout 2 * periodTime in ns
    serialPort.setRxTimeout((int64_t)periodTime * 2000000llu);

    // set values
    utcTime               = 0.0f;
    utcTimeOld            = 0.0f;
    satelliteNumOld       = 0;
    gpsData.recordingTime = rackTime.get();

    return RackDataModule::moduleOn(); // has to be last command in moduleOn();
}

// realtime context
void GpsNmea::moduleOff(void)
{
    RackDataModule::moduleOff();       // has to be first command in moduleOff();
}

// realtime context
int GpsNmea::moduleLoop(void)
{
    int                 ret;
    int                 nmeaMsg = -1;
    gps_data*           p_data;
    position_wgs84_data posWgs84Data;
    position_data       posData;

    // get datapointer from rackdatabuffer
    p_data = (gps_data *)getDataBufferWorkSpace();

    // read next NMEA message
    ret = readNMEAMessage();

    if (!ret)
    {
        // decode NMEA message type
        if (strstr(&nmea.data[0], "GPRMC") != NULL)
        {
            nmeaMsg = RMC_MSG;
        }
        if (strstr(&nmea.data[0], "GPGGA") != NULL)
        {
            nmeaMsg = GGA_MSG;
        }
        if (strstr(&nmea.data[0], "GPGSA") != NULL)
        {
            nmeaMsg = GSA_MSG;
        }
        if (strstr(&nmea.data[0], "VTG") != NULL)
        {
            nmeaMsg = VTG_MSG;
        }

        // write package if a complete dataset is read
        if (nmeaMsg == trigMsg)
        {
            // gps data invalid by utcTime
            if ((utcTime != 0) && (utcTimeOld != 0) && (utcTime == utcTimeOld))
            {
                GDOS_ERROR("Gps invalid by utcTime: %f, %f\n", utcTime, utcTimeOld);

                gpsData.mode          = GPS_MODE_INVALID;
                gpsData.latitude      = 0.0;
                gpsData.longitude     = 0.0;
                gpsData.altitude      = 0;
                gpsData.heading       = 0.0f;
                gpsData.speed         = 0;
                gpsData.satelliteNum  = 0;
                gpsData.utcTime       = 0;
                gpsData.pdop          = 0.0f;
                gpsData.pos.x         = 0;
                gpsData.pos.y         = 0;
                gpsData.pos.z         = 0;
                gpsData.pos.phi       = 0.0f;
                gpsData.pos.psi       = 0.0f;
                gpsData.pos.rho       = 0.0f;
                gpsData.varXY         = (int)1e12;
                gpsData.varZ          = (int)1e12;
                gpsData.varRho        = 1e12;
            }

            // gps data valid
            else
            {
                // calculate position and orientation in global cartesian coordinates
                posWgs84Data.latitude  = gpsData.latitude;
                posWgs84Data.longitude = gpsData.longitude;
                posWgs84Data.altitude  = gpsData.altitude;
                posWgs84Data.heading   = gpsData.heading;

                position->wgs84ToPos(&posWgs84Data, &posData);

                memcpy(&gpsData.pos, &posData.pos, sizeof(position_3d));
                gpsData.pos.rho = normaliseAngle(atan2(posData.pos.y - posDataOld.pos.y,
                                                       posData.pos.x - posDataOld.pos.x));
                memcpy(&posDataOld, &posData, sizeof(position_data));

                // estimate GPS position variance
                if (gpsData.satelliteNum >= 6)
                {
                    gpsData.varXY  = varXY;               // default 20m
                    gpsData.varZ   = varXY;               // default 100m
                }
                else if (gpsData.satelliteNum >= 4)
                {
                    gpsData.varXY  = varXY * 5;           // default 100m
                    gpsData.varZ   = varXY * 5 * 5;       // default 500m
                }
                else if (gpsData.satelliteNum >= 3)
                {
                    gpsData.varXY  = varXY * 5;           // default 100m
                    gpsData.varZ   = (int)1e12;           // Mode 2D
                }
                else  // satelliteNum < 3
                {
                    gpsData.varXY  = (int)1e12;
                    gpsData.varZ   = (int)1e12;
                }


                // estimate GPS heading variance
                if ((gpsData.satelliteNum >= 4) & (gpsData.satelliteNum == satelliteNumOld))
                {
                    // use gps heading only with motion of at least 0.3m/s
                    if (gpsData.speed > 300)
                    {
                        gpsData.varRho = varRho;          // default 90 deg
                    }
                    else
                    {
                        gpsData.varRho = 1e12;       // no GPS heading
                    }
                }
                else
                {
                    gpsData.varRho = 1e12;           // no GPS heading
                }
            }

            memcpy(p_data, &gpsData, sizeof(gps_data));
            putDataBufferWorkSpace(sizeof(gps_data));

            utcTimeOld            = utcTime;
            satelliteNumOld       = gpsData.satelliteNum;
            gpsData.recordingTime = nmea.recordingTime;
        }

        // RMC - Message
        if (nmeaMsg == RMC_MSG)
        {
            if (analyseRMC(&gpsData) == 0)
            {
                GDOS_DBG_DETAIL("received RMC message, recordingTime %i\n", nmea.recordingTime);
            }
        }

        // GGA - Message
        else if (nmeaMsg == GGA_MSG)
        {
            if (analyseGGA(&gpsData) == 0)
            {
                GDOS_DBG_DETAIL("received GGA message, recordingTime %i\n", nmea.recordingTime);
            }
        }

        // GSA - Message
        else if (nmeaMsg == GSA_MSG)
        {
            if (analyseGSA(&gpsData) == 0)
            {
                GDOS_DBG_DETAIL("received GSA message, recordingTime %i\n", nmea.recordingTime);
            }
        }

        // VTG - Message
        else if (nmeaMsg == VTG_MSG)
        {
            if (analyseVTG(&gpsData) == 0)
            {
                GDOS_DBG_DETAIL("received VTG message, recordingTime %i\n", nmea.recordingTime);
            }
        }

        else
        {
            GDOS_DBG_DETAIL("received unknown message, recordingTime %i\n", nmea.recordingTime);
        }
    }
    else
    {
        GDOS_WARNING("Can't read data from serial device %i, code %i\n", serialDev, ret);
    }


    // gps data timeout condition
    if (((int)rackTime.get() - (int)gpsData.recordingTime) >= (1.5 * (int)periodTime))
    {
        GDOS_DBG_DETAIL("no position fix available or invalid trigger message\n");

        gpsData.recordingTime = rackTime.get();
        gpsData.mode          = GPS_MODE_INVALID;
        gpsData.latitude      = 0.0;
        gpsData.longitude     = 0.0;
        gpsData.altitude      = 0;
        gpsData.heading       = 0.0f;
        gpsData.speed         = 0;
        gpsData.satelliteNum  = 0;
        gpsData.utcTime       = 0;
        gpsData.pdop          = 0.0f;
        gpsData.pos.x         = 0;
        gpsData.pos.y         = 0;
        gpsData.pos.z         = 0;
        gpsData.pos.phi       = 0.0f;
        gpsData.pos.psi       = 0.0f;
        gpsData.pos.rho       = 0.0f;
        gpsData.varXY         = (int)1e12;
        gpsData.varZ          = (int)1e12;
        gpsData.varRho        = 1e12;

        memcpy(p_data, &gpsData, sizeof(gps_data));
        putDataBufferWorkSpace(sizeof(gps_data));

        satelliteNumOld = gpsData.satelliteNum;
    }

    return 0;
}

int GpsNmea::moduleCommand(message_info *msgInfo)
{
    // not for me -> ask RackDataModule
    return RackDataModule::moduleCommand(msgInfo);
}


/*****************************************************************************
* This function reads a single NMEA-Message, delimited by the Line Feed      *
* character (0x0A) from the serial device. The characters and the            *
* recordingtime of the first character ('$') are saved to the nmea_data      *
* structure. This structure will be analyzed by the specific functions,      *
* like analyseRMC.                                                              *
******************************************************************************/
int GpsNmea::readNMEAMessage()
{
    int             i, ret;
    int             msgSize;
    unsigned char   currChar;
    rack_time_t       recordingTime;


    // Initalisation of local variables
    recordingTime   = 0;
    currChar        = 0;
    msgSize         = sizeof(nmea.data);
    i               = 0;

    // Synchronize to message head, timeout after 200 attempts
    while ((i < 200) && (currChar != '$'))
    {
        // Read next character
        ret = serialPort.recv(&currChar, 1, &recordingTime);
        if (ret)
        {
            GDOS_ERROR("Can't read data from serial dev %i, code = %d\n",
                       serialDev, ret);
            return ret;
        }
        i++;
    }


    // Check for timeout
    if (i >= 200)
    {
        GDOS_ERROR("Can't synchronize on package head\n");
        return -ETIME;
    }
    // First character of NMEA-Message -> set recordingTime
    else
    {
        i = 0;
        nmea.recordingTime = recordingTime;
    }


    // Read NMEA-Message until currChar == "Line-Feed",
    // timout if msgSize is reached
    while ((i < msgSize) && (currChar != 0x0A))
    {
        // Read next character
        ret = serialPort.recv(&currChar, 1, &recordingTime);
        if (ret)
        {
            GDOS_ERROR("Can't read data from serial dev %i, code = %d\n",
                       serialDev, ret);
            return ret;
        }

        // Store last character
        nmea.data[i] = currChar;
        i++;
    }


    // if last read character != "Line-Feed" an error occured
    if (currChar != 0x0A)
    {
        GDOS_ERROR("Can't read end of NMEA message\n");
        return -EINVAL;
    }
    else
          return 0;
}

/*****************************************************************************
* This function analyses the NMEA "RMC"-Message.                             *
*                                                                            *
*  0:    GPRMC                          Protokoll header                     *
*  1:    hhmmss.dd                      UTC time                             *
*  2:    S                              Status indicator (A = valid /        *
*                                                       V = invalid)         *
*  3:    xxmm.dddd                      Latitude                             *
*  4:    <N|S>                          North/ south indicator               *
*  5:    yyymm.dddd                     Longitude                            *
*  6:    <E|W>                          East/ west indicator                 *
*  7:    s.s                            Speed in knots                       *
*  8:    h.h                            Heading                              *
*  9:    ddmmyy                         Date                                 *
* 10:    d.d                            Magnetic variation                   *
* 11:    <E|W>                          Declination                          *
* 12:    M                              Mode indicator (A = autonomous /     *
*                                                       N = data not valid)  *
* 13:    Checksum                                                            *
* 14:    <CR LF>                                                             *
******************************************************************************/
int GpsNmea::analyseRMC(gps_data *data)
{
    char            buffer[20];
    char            currChar;
    unsigned char   checksum;
    int             pos, i, j;
    int             date;
    float           fNum;
    double          dNum;

    // Initalisation of local variables
    i = 0;
    currChar   = 0;
    checksum   = 0;
    pos        = 0;

    // Decode message, until checksum delimiter or timeout condition is reached
    while ((i <= 14) && (currChar != '*'))
    {
        // Decode message-part (max. 20 chars per part)
        for (j = 0; j < 20; j++)
        {
            // read next char
            currChar = nmea.data[pos];
            pos++;

            // calc new checksum until checksum delimiter is reached
            if (currChar != '*')
                checksum = checksum ^ currChar;

            // save character if current char is no delimiter
            if ((currChar == ',') || (currChar == '*') || (currChar == 0x0D))
            {
                buffer[j] = 0;
                break;
            }
            else
                buffer[j]  = currChar;
        }
       
         if (strlen(buffer) > 0)
        {
            // Decode message
            switch(i)
            {
                // UTC-Time [hhmmss.dd]
                case 1:
                    sscanf(buffer, "%f", &utcTime);
                    break;
    
                // Latitude [xxmm.dddd]
                case 3:
                    sscanf(buffer, "%lf", &dNum);
                    data->latitude = degHMStoRad(dNum);
                    break;
    
                // Latitude north / south adjustment [N|S]
                case 4:
                    if (buffer[0] == 'S')
                        data->latitude *= -1.0;
                    break;
    
                // Longitude [yyymm.dddd]
                case 5:
                    sscanf(buffer, "%lf", &dNum);
                    data->longitude = degHMStoRad(dNum);
                    break;
    
                // Longitude east / west adjustment [E|W]
                case 6:
                    if (buffer[0] == 'W')
                        data->longitude *= -1.0;
                    break;
    
                // Speed [s.s]
                case 7:
                    sscanf(buffer, "%f", &fNum);
                    data->speed = (int)rint(fNum * KNOTS_TO_MS * 1000.0);
                    break;
    
                // Heading[h.h]
                case 8:
                    sscanf(buffer, "%f", &fNum);
                    data->heading = fNum * M_PI / 180.0;
                    break;
    
                // Date [ddmmyy]
                case 9:
                    sscanf(buffer, "%d", &date);
                    data->utcTime = toCalendarTime(utcTime, date);
                    break;
    
                default:
                    break;
            }
        }
        i++;
    }

    // compare checksum
    if (currChar == '*')
    {
        if (strtol(&nmea.data[pos], NULL, 16) == checksum)
            return 0;
        else
          {
              GDOS_ERROR("RMC: Wrong checksum\n");
                return -EINVAL;
          }
      }
      else
      {
          GDOS_ERROR("RMC: Wrong NMEA-format\n");
            return -EINVAL;
      }
}


/******************************************************************************
* This function analyses the "GGA"-Message.                                   *
*                                                                             *
*  0:    GPGGA                          Protokoll header                      *
*  1:    hhmmss.dd                      UTC time                              *
*  2:    xxmm.dddd                      Latitude                              *
*  3:    <N|S>                          North/ south indicator                *
*  4:    yyymm.dddd                     Longitude                             *
*  5:    <E|W>                          East/ west indicator                  *
*  6:    v                              Fix valid indicator                   *
*                                       (0 = fix not valid / 1 = fix valid)   *
*  7:    ss                             Number of satellites used in          *
*                                       position fix (00 - 12)                *
*  8:    d.d                            HDOP                                  *
*  9:    h.h                            Altitude (mean-sea-level, geoid)      *
* 10:    M                              Letter M                              *
* 11:    g.g                            Difference between WGS84 ellipsoid    *
*                                       and mean-sea-level altitude           *
* 12:    M                              Letter M                              *
* 13:    a.a                            NULL(missing)                         *
* 14:    xxxx                           NULL(missing)                         *
* 15:    Checksum                                                             *
* 16:    <CR LF>                                                              *
*******************************************************************************/
int GpsNmea::analyseGGA(gps_data *data)
{
    char            buffer[20];
    char            currChar;
    unsigned char   checksum;
    int             pos, i, j;
    float           fNum;
    double          dNum;

    // Initalisation of local variables
    i = 0;
    currChar   = 0;
    checksum   = 0;
    pos        = 0;

    // Decode message, until checksum delimiter or timeout condition is reached
    while ((i <= 16) && (currChar != '*'))
    {
        // Decode message-part (max. 20 chars per part)
        for (j = 0; j < 20; j++)
        {
            // read next char
            currChar = nmea.data[pos];
            pos++;

            // calc new checksum until checksum delimiter is reached
            if (currChar != '*')
                checksum = checksum ^ currChar;

            // save character if current char is no delimiter
            if ((currChar == ',') || (currChar == '*') || (currChar == 0x0D))
            {
                buffer[j] = 0;
                break;
            }
            else
                buffer[j]  = currChar;
        }

        if (strlen(buffer) > 0)
        {
            // Decode message
            switch(i)
            {
                // UTC-Time [hhmmss.dd]
                case 1:
                    sscanf(buffer, "%f", &utcTime);
                    break;
    
                // Latitude [xxmm.dddd]
                case 2:
                    sscanf(buffer, "%lf", &dNum);
                    data->latitude = degHMStoRad(dNum);
                    break;
    
                // Latitude north / south adjustment [N|S]
                case 3:
                    if (buffer[0] == 'S')
                        data->latitude *= -1.0;
                    break;
    
                // Longitude [yyymm.dddd]
                case 4:
                    sscanf(buffer, "%lf", &dNum);
                    data->longitude = degHMStoRad(dNum);
                    break;
    
                // Longitude east / west adjustment [E|W]
                case 5:
                    if (buffer[0] == 'W')
                        data->longitude *= -1.0;
                    break;
    
                // Number of satellites used in position fix
                case 7:
                    sscanf(buffer, "%d", &data->satelliteNum);
                    break;
    
                // Altitude [h.h]
                case 9:
                    sscanf(buffer, "%f", &fNum);
                    data->altitude = (int)rint(fNum * 1000.0f);     // in mm
                    break;
    
                default:
                    break;
            }
        }

        i++;
    }

    // compare checksum
    if (currChar == '*')
    {
        if (strtol(&nmea.data[pos], NULL, 16) == checksum)
            return 0;
        else
        {
            GDOS_ERROR("GGA: Wrong checksum\n");
            return -EINVAL;
        }
    }
    else
    {
        GDOS_ERROR("GGA: Wrong NMEA-format\n");
        return -EINVAL;
    }
}


/*****************************************************************************
* This function analyses the "GSA"-Message.                                  *
*                                                                            *
*  0:    GPGSA                     Protokoll header                          *
*  1:    a                         Mode (M = manual 2D-3D operation /        *
*                                        A = automatic 2D-3D operation)      *
*  2:    b                         Mode (1 = fix not available /             *
*                                        2 = 2D / 3 = 3D)                    *
*  3:    xx                        PRN num of satellite used in solution     *
*  4:    xx                        PRN num of satellite used in solution     *
*  5:    xx                        PRN num of satellite used in solution     *
*  6:    xx                        PRN num of satellite used in solution     *
*  7:    xx                        PRN num of satellite used in solution     *
*  8:    xx                        PRN num of satellite used in solution     *
*  9:    xx                        PRN num of satellite used in solution     *
* 10:    xx                        PRN num of satellite used in solution     *
* 11:    xx                        PRN num of satellite used in solution     *
* 12:    xx                        PRN num of satellite used in solution     *
* 13:    xx                        PRN num of satellite used in solution     *
* 14:    xx                        PRN num of satellite used in solution     *
* 15:    p.p                       PDOP                                      *
* 16:    h.h                       HDOP                                      *
* 17:    v.v                       VDOP                                      *
* 18:    Checksum                                                            *
* 19:    <CR LF>                                                             *
******************************************************************************/
int GpsNmea::analyseGSA(gps_data *data)
{
    char            buffer[20];
    char            currChar;
    unsigned char   checksum;
    int             pos, i, j;

    // Initalisation of local variables
    i = 0;
    currChar   = 0;
    checksum   = 0;
    pos        = 0;

    // Decode message, until checksum delimiter or timeout condition is reached
    while ((i <= 19) && (currChar != '*'))
    {
        // Decode message-part (max. 20 chars per part)
        for (j = 0; j < 20; j++)
        {
            // read next char
            currChar = nmea.data[pos];
            pos++;

            // calc new checksum until checksum delimiter is reached
            if (currChar != '*')
                checksum = checksum ^ currChar;

            // save character if current char is no delimiter
            if ((currChar == ',') || (currChar == '*') || (currChar == 0x0D))
            {
                buffer[j] = 0;
                break;
            }
            else
                buffer[j]  = currChar;
        }

        if (strlen(buffer) > 0)
        {
            // Decode message
            switch(i)
            {
                // Mode (1 = fix not valid / 2 = 2D / 3 = 3D)
                case 2:
                    sscanf(buffer, "%d", &data->mode);
                    break;
    
                // PDOP
                case 15:
                    sscanf(buffer, "%f", &data->pdop);
                    break;
    
                default:
                    break;
            }
        }
        i++;
    }

    // compare checksum
    if (currChar == '*')
    {
        if (strtol(&nmea.data[pos], NULL, 16) == checksum)
            return 0;
        else
        {
            GDOS_ERROR("GSA: Wrong checksum\n");
            return -EINVAL;
        }
    }
    else
    {
        GDOS_ERROR("GSA: Wrong NMEA-format\n");
        return -EINVAL;
    }
}


/*****************************************************************************
* This function analyses the "VTG"-Message.                                  *
*                                                                            *
*  0:    GPVTG                     Protokoll header                          *
*  1:    h.h                       Heading                                   *
*  2:    T                         Degrees (heading units)                   *
*  3:    m.m                       Magnetic heading                          *
*  4:    M                         Degrees (magnetic heading units)          *
*  5:    s.s                       Speed knots                               *
*  6:    N                         Knots (speed unit)                        *
*  7:    s.s                       Speed km/h                                *
*  8:    K                         Km/h (speed unit)                         *
*  9:    M                         Mode indicator                            *
* 18:    Checksum                                                            *
* 19:    <CR LF>                                                             *
******************************************************************************/
int GpsNmea::analyseVTG(gps_data *data)
{
    char            buffer[20];
    char            currChar;
    unsigned char   checksum;
    int             pos, i, j;
    float           fNum;

    // Initalisation of local variables
    i = 0;
    currChar   = 0;
    checksum   = 0;
    pos        = 0;

    // Decode message, until checksum delimiter or timeout condition is reached
    while ((i <= 19) && (currChar != '*'))
    {
        // Decode message-part (max. 20 chars per part)
        for (j = 0; j < 20; j++)
        {
            // read next char
            currChar = nmea.data[pos];
            pos++;

            // calc new checksum until checksum delimiter is reached
            if (currChar != '*')
                checksum = checksum ^ currChar;

            // save character if current char is no delimiter
            if ((currChar == ',') || (currChar == '*') || (currChar == 0x0D))
            {
                buffer[j] = 0;
                break;
            }
            else
                buffer[j]  = currChar;
        }

        if (strlen(buffer) > 0)
        {
            // Decode message
            switch(i)
            {
                // Heading[h.h]
                case 1:
                    sscanf(buffer, "%f", &fNum);
                    data->heading = fNum * M_PI / 180.0;
                    break;
    
                // Speed [s.s] knots
                case 5:
                    sscanf(buffer, "%f", &fNum);
                    data->speed = (int)rint(fNum * KNOTS_TO_MS * 1000.0);
                    break;
    
                // Speed [s.s] km/h
                case 7:
                    sscanf(buffer, "%f", &fNum);
                    data->speed = (int)rint(fNum * 1000.0 / 3.6);
                    break;
    
                default:
                    break;
            }
        }
        i++;
    }

    // compare checksum
    if (currChar == '*')
    {
        if (strtol(&nmea.data[pos], NULL, 16) == checksum)
            return 0;
        else
        {
            GDOS_ERROR("GSA: Wrong checksum\n");
            return -EINVAL;
        }
    }
    else
    {
        GDOS_ERROR("GSA: Wrong NMEA-format\n");
        return -EINVAL;
    }
}


double GpsNmea::degHMStoRad(double degHMS)
{
    int deg;
    double min;

    deg = (int)(degHMS / 100);
    min = (degHMS - (double)deg * 100.0);

    return ((double)deg + (min / 60.0)) * M_PI / 180.0;
}

long GpsNmea::toCalendarTime(float time, int date)
{
    int        hour, min, sec;
    int        year, mon, day;
    float      fdiff;
    int        idiff;

    // time
    hour   = (int)(time / 10000.0f);
    fdiff  = time - hour * 10000;
    min    = (int)(fdiff / 100.0f);
    fdiff -= min * 100;
    sec    = (int)rint(fdiff);

    // date
    day    = date / 10000;
    idiff  = date - day * 10000;
    mon    = (idiff / 100) - 1;
    idiff -= mon * 100;
    year   = idiff + 1900;

    if (0 >= (int)(mon -= 2))
    {
        mon  += 12;              // Puts Feb last since it has leap day
        year -= 1;
    }

    return ((((long)(year/4 - year/100 + year/400 + 367*mon/12 + day) +
                     year*365 - 719499
                                      )*24 + hour // hours
                                      )*60 + min // minutes
                                      )*60 + sec; // seconds
}


/*****************************************************************************
* The global position "posLla", given by latitude, longitude and altitude in *
* WGS84 coordinates is transformed into Gauss-Krueger coordinates "posGk".   *
* Unit of the Gauss-Krueger coordinates is meter.                            *
******************************************************************************/
void GpsNmea::posWGS84ToGK(gps_nmea_pos_3d *posLLA, gps_nmea_pos_3d *posGK)
{
    double l1, l2, b1, b2, h1, h2;
    double r, h, a, b, eq, n;
    double xq, yq, zq, x, y, z;
    const double awgs = 6378137.0;        // WGS84 Semi-Major Axis =
                                          // Equatorial Radius in meter
    const double bwgs = 6356752.31425;    // WGS84 Semi-Minor Axis =
                                          // Polar Radius in meter
    const double abes = 6377397.155;      // Bessel Semi-Major Axis =
                                          // Equatorial Radius in meter
    const double bbes = 6356078.962;      // Bessel Semi-Minor Axis =
                                          // Polar Radius in meter

    b1  = posLLA->x;
    l1  = posLLA->y;
    h1  = posLLA->z;

    a   = awgs;
    b   = bwgs;
    eq  = (a*a - b*b) / (a*a);
    n   = a / sqrt(1 - eq*sin(b1)*sin(b1));
    xq  = (n + h1)*cos(b1)*cos(l1);
    yq  = (n + h1)*cos(b1)*sin(l1);
    zq  = ((1 - eq)*n + h1)*sin(b1);
    helmertTransformation(xq, yq, zq, &x, &y, &z);

    a   = abes;
    b   = bbes;
    bLRauenberg(x, y, z, &b2, &l2, &h2);
      besselBLToGaussKrueger(b2, l2, &r, &h);

    b2  = b2 * 180.0 / M_PI;
    l2  = l2 * 180.0 / M_PI;

    posGK->x = h;
    posGK->y = r;
    posGK->z = h1;
}

void GpsNmea::helmertTransformation(double x, double y, double z,
                                    double *xo, double *yo, double *zo)
{
    const double rotx = 2.540423689E-6;   // Rotation Parameter 1
    const double roty = 7.514612057E-7;   // Rotation Parameter 2
    const double rotz = -1.368144208E-5;  // Rotation Parameter 3
    const double sc   = 1.0 / 0.99999122; // Scaling Factor
    const double dx   = -585.7;           // Translation Parameter 1
    const double dy   = -87.0;            // Translation Parameter 2
    const double dz   = -409.2;           // Translation Parameter 3

    *xo = dx + (sc*(1*x + rotz*y - roty*z));
    *yo = dy + (sc*(-rotz*x + 1*y + rotx*z));
    *zo = dz + (sc*(roty*x - rotx*y + 1*z));
}

void GpsNmea::besselBLToGaussKrueger(double b, double ll,
                                     double *re, double *ho)
{
    double l, l0, bg, lng, ng;
    double k, t, eq, vq, v, nk;
    double x, gg, ss, y, kk, rvv;
    const double abes = 6377397.155;      // Bessel Semi-Major Axis =
                                          // Equatorial Radius in meter
    const double bbes = 6356078.962;      // Bessel Semi-Minor Axis =
                                          // Polar Radius in meter
    const double cbes = 111120.6196;      // Bessel latitude to Gauss-Krueger
                                          // in meter

    bg  = 180.0 * b / M_PI;
    lng = 180.0 * ll / M_PI;
    l0  = 3.0 * rint((180.0 * ll / M_PI) / 3.0);
    l0  = M_PI * l0 / 180.0;
    l   = ll - l0;
    k   = cos(b);
    t   = sin(b) / k;
    eq  = (abes*abes - bbes*bbes) / (bbes*bbes);
    vq  = 1.0 + eq*k*k;
    v   = sqrt(vq);
    ng  = abes*abes / (bbes*v);
    nk  = (abes - bbes) / (abes + bbes);
    x   = ((ng*t*k*k*l*l) / 2.0) +
          ((ng*t*(9.0*vq - t*t - 4.0)*k*k*k*k*l*l*l*l) / 24.0);

    gg  = b + (((-3.0*nk / 2.0) + (9.0*nk*nk*nk / 16.0))*sin(2.0*b) +
          15*nk*nk*sin(4.0*b) / 16.0 - 35.0*nk*nk*nk*sin(6.0*b) / 48.0);
    ss  = gg * 180.0 * cbes / M_PI;
    *ho = ss + x;
    y   = ng*k*l + ng*(vq - t*t)*k*k*k*l*l*l / 6.0 +
          ng*(5.0 - 18.0*t*t + t*t*t*t)*k*k*k*k*k*l*l*l*l*l / 120.0;
    kk  = 500000.0;
    rvv = rint((180.0 * ll / M_PI) / 3.0);
    *re =   rvv * 1000000.0 + kk + y;
}


void GpsNmea::bLRauenberg(double x, double y, double z,
                          double *b, double *l, double *h)
{
    double f, f1, f2, ft;
    double p, eq;
    const double abes = 6377397.155;      // Bessel Semi-Major Axis =
                                          // Equatorial Radius in meter
    const double bbes = 6356078.962;      // Bessel Semi-Minor Axis =
                                          // Polar Radius in meter

    f  = M_PI * 50.0 / 180.0;
    p  = z / sqrt(x*x + y*y);
    eq = (abes*abes - bbes*bbes) / (abes*abes);

    do
    {
        f1 = newF(f, x, y, p, eq, abes);
        f2 = f;
        f  = f1;
        ft = 180.0 * f1 / M_PI;
    }
    while(!(fabs(f2 - f1) < 10E-10));

    *b = f;
    *l = atan(y/x);
    *h = sqrt(x*x + y*y)/cos(f1) - abes/
         sqrt(1 - eq * sin(f1) * sin(f1));
}

double GpsNmea:: newF(double f, double x, double y,
                      double p, double eq, double a)
{
    double zw, nnq;

    zw  = a / sqrt(1 - eq * sin(f) * sin(f));
    nnq = 1 - eq * zw / (sqrt(x*x + y*y) / cos(f));

    return atan(p / nnq);
}


/*******************************************************************************
 *   !!! NON REALTIME CONTEXT !!!
 *
 *   moduleInit,
 *   moduleCleanup,
 *   Constructor,
 *   Destructor,
 *   main,
 *
 *   own non realtime user functions
 ******************************************************************************/
int GpsNmea::moduleInit(void)
{
    int ret;

    // call RackDataModule init function (first command in init)
    ret = RackDataModule::moduleInit();
    if (ret)
    {
        return ret;
    }
    initBits.setBit(INIT_BIT_DATA_MODULE);

    ret = serialPort.open(serialDev, &gps_serial_config, this);
    if (ret)
    {
        GDOS_ERROR("Can't open serialDev %i, code=%d\n", serialDev, ret);
        goto init_error;
    }
    GDOS_DBG_INFO("serialDev %d has been opened \n", serialDev);
    initBits.setBit(INIT_BIT_SERIALPORT_OPEN);

    //
    // create mailboxes
    //

    // work mailbox
    ret = createMbx(&workMbx, 1, 128,
                    MBX_IN_KERNELSPACE | MBX_SLOT);
    if (ret)
    {
        goto init_error;
    }
    initBits.setBit(INIT_BIT_MBX_WORK);

    //
    // create Proxies
    //

    // position
    position = new PositionProxy(&workMbx, 0, positionInst);
    if (!position)
    {
        ret = -ENOMEM;
        goto init_error;
    }
    initBits.setBit(INIT_BIT_PROXY_POSITION);


    return 0;

init_error:
    moduleCleanup();
    return ret;
}

void GpsNmea::moduleCleanup(void)
{
    int ret;

    // free position proxy
    if (initBits.testAndClearBit(INIT_BIT_PROXY_POSITION))
    {
        delete position;
    }

    // delete work mailbox
    if (initBits.testAndClearBit(INIT_BIT_MBX_WORK))
    {
        destroyMbx(&workMbx);
    }

    if (initBits.testAndClearBit(INIT_BIT_SERIALPORT_OPEN))
    {
        ret = serialPort.close();
        if (ret)
        {
            GDOS_WARNING("Can't close serialDev %i, code=%d\n", serialDev, ret);
        }
    }

    // call RackDataModule cleanup function (last command in cleanup)
    if (initBits.testAndClearBit(INIT_BIT_DATA_MODULE))
    {
        RackDataModule::moduleCleanup();
    }
}

GpsNmea::GpsNmea()
        : RackDataModule( MODULE_CLASS_ID,
                      5000000000llu,    // 5s cmdtask error sleep time
                      5000000000llu,    // 5s datatask error sleep time
                      100000000llu,     // 100ms datatask disable sleep time
                      16,               // command mailbox slots
                      48,               // command mailbox data size per slot
                      MBX_IN_KERNELSPACE | MBX_SLOT, // command mailbox flags
                      5,                // max buffer entries
                      10)               // data buffer listener
{
    // get value(s) out of your argument table
    positionInst = getIntArg("positionInst", argTab);
    serialDev    = getIntArg("serialDev", argTab);
    periodTime   = getIntArg("periodTime", argTab);
    trigMsg      = getIntArg("trigMsg", argTab);
    posGKOffsetX = getIntArg("posGKOffsetX", argTab);
    posGKOffsetY = getIntArg("posGKOffsetY", argTab);
    gps_serial_config.baud_rate  = getIntArg("baudrate", argTab);
    varXY        = getIntArg("varXY", argTab);
    varRho       = (float)(getIntArg("varRho", argTab) * M_PI / 180.0);

    // set dataBuffer size
    setDataBufferMaxDataSize(sizeof(gps_data));

    // set databuffer period time
    setDataBufferPeriodTime(periodTime);
}

int main(int argc, char *argv[])
{
    int ret;

    // get args
    ret = RackModule::getArgs(argc, argv, argTab, "GpsNmea");
    if (ret)
    {
        printf("Invalid arguments -> EXIT \n");
        return ret;
    }

    // create new GpsNmea
    p_inst = new GpsNmea();
    if (!p_inst)
    {
        printf("Can't create new GpsNmea -> EXIT\n");
        return -ENOMEM;
    }

    // init
    ret = p_inst->moduleInit();
    if (ret)
        goto exit_error;

    p_inst->run();

    return 0;

exit_error:

    delete (p_inst);

    return ret;
}
