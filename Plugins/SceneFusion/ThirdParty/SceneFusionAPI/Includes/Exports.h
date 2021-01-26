/*************************************************************************
 *
 * KINEMATICOUP CONFIDENTIAL
 * __________________
 *
 *  Copyright (2017-2020) KinematicSoup Technologies Incorporated
 *  All Rights Reserved.
 *
 * NOTICE:  All information contained herein is, and remains
 * the property of KinematicSoup Technologies Incorporated and its
 * suppliers, if any.  The intellectual and technical concepts contained
 * herein are proprietary to KinematicSoup Technologies Incorporated
 * and its suppliers and may be covered by Canadian and Foreign Patents,
 * patents in process, and are protected by trade secret or copyright law.
 * Dissemination of this information or reproduction of this material
 * is strictly forbidden unless prior written permission is obtained
 * from KinematicSoup Technologies Incorporated.
 */
#pragma once

#ifndef KS_DLL
    #define EXTERNAL
    #define INTERNAL
#else
    #ifdef _WIN32    //Windows
        #ifdef SF_EXPORTS
            #ifdef SF_TEST
                #define INTERNAL __declspec(dllexport)
            #else
                #define INTERNAL
            #endif
    
            #define EXTERNAL __declspec(dllexport)
        #else
            #ifdef SF_TEST
                #define INTERNAL __declspec(dllimport)
            #else
                #define INTERNAL
            #endif
    
            #define EXTERNAL __declspec(dllimport)
        #endif 
    #else       //Non windows
        #ifdef SF_EXPORTS
            #ifdef SF_TEST
                #define INTERNAL
            #else
                #define INTERNAL __attribute__((__visibility__("hidden")))
            #endif
        #else
            #define INTERNAL
        #endif 

        #define EXTERNAL
    #endif
#endif