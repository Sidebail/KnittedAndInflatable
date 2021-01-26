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
#include <memory>
#include <string>
#include <ksColor.h>
#include <Exports.h>

namespace KS {
namespace SceneFusion2 {
    /**
     * A user who is connected to the session.
     */
    class EXTERNAL sfUser :
        public std::enable_shared_from_this<sfUser>
    {
    public:
        typedef std::shared_ptr<sfUser> SPtr;

        /**
         * Destructor
         */
        virtual ~sfUser() {};

        /**
         * Gets the user ID.
         *
         * @return  const uint32_t&
         */
        virtual const uint32_t& Id() const = 0;

        /**
         * Gets the user name.
         *
         * @return  const std::string&
         */
        virtual const std::string& Name() const = 0;

        /**
         * Get the user color.
         *
         * @return  const ksColor&
         */
        virtual const ksColor& Color() const = 0;

        /**
         * Is the user the local user.
         *
         * @return  bool
         */
        virtual bool IsLocal() const = 0;
    };
} // SceneFusion2
} // KS