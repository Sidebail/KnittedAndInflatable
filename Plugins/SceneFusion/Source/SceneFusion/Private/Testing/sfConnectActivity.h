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

#include "sfBaseActivity.h"

/**
 * Activity for connecting and disconnecting from a local session.
 */
class sfConnectActivity : public sfBaseActivity
{
public:
    /**
     * Constructor
     *
     * @param   const FString& name of activity.
     * @param   float weight the determines how likely this activity is to occur.
     */
    sfConnectActivity(const FString& name, float weight);

    /**
     * Handles command arguments. The first argument is the host address and the optional second argument is the port.
     * Alternatively both host and port can be in one argument of the form host:port.
     *
     * @param   const TArray<FString>& args
     * @param   int index of first arg in array to process.
     */
    virtual void HandleArgs(const TArray<FString>& args, int index) override;

    /**
     * If connected to a session, disconnects. Otherwise connects to a local session.
     */
    virtual void Start() override;

private:
    std::string m_host;
    uint16_t m_port;
};