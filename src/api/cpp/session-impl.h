/*
 * This file is part of the GROMACS molecular simulation package.
 *
 * Copyright (c) 2018, by the GROMACS development team, led by
 * Mark Abraham, David van der Spoel, Berk Hess, and Erik Lindahl,
 * and including many others, as listed in the AUTHORS file in the
 * top-level source directory and at http://www.gromacs.org.
 *
 * GROMACS is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1
 * of the License, or (at your option) any later version.
 *
 * GROMACS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with GROMACS; if not, see
 * http://www.gnu.org/licenses, or write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA.
 *
 * If you want to redistribute modifications to GROMACS, please
 * consider that scientific software is very special. Version
 * control is crucial - bugs must be traceable. We will be happy to
 * consider code for inclusion in the official distribution, but
 * derived work must not be called official GROMACS. Details are found
 * in the README & COPYING files - if they are missing, get the
 * official version at http://www.gromacs.org.
 *
 * To help us fund GROMACS development, we humbly ask that you cite
 * the research papers on the package. Check out http://www.gromacs.org.
 */

#ifndef GMXAPI_SESSION_IMPL_H
#define GMXAPI_SESSION_IMPL_H
/*! \file
 * \brief Declare implementation interface for Session API class(es).
 *
 * \ingroup gmxapi
 */

#include <map>

#include "gromacs/mdrun/logging.h"
#include "gromacs/mdrun/runner.h"
#include "gromacs/mdrun/simulationcontext.h"

#include "gmxapi/context.h"
#include "gmxapi/status.h"

namespace gmxapi
{

// Forward declaration
class MpiContextManager; // Locally defined in session.cpp
class ContextImpl;       // locally defined in context.cpp

/*!
 * \brief Implementation class for executing sessions.
 *
 * Since 0.0.3, there is only one context and only one session type. This may
 * change at some point to allow templating on different resource types or
 * implementations provided by different libraries.
 * \ingroup gmxapi
 */
class SessionImpl
{
    public:
        //! Use create() factory to get an object.
        SessionImpl() = delete;
        ~SessionImpl();

        /*!
         * \brief Check if the session is (still) running.
         *
         * When a session is launched, it should be returned in an "open" state by the launcher function.
         * \return True if running, false if already closed.
         */
        bool isOpen() const noexcept;

        /*!
         * \brief Explicitly close the session.
         *
         * Sessions should be explicitly `close()`ed to allow for exceptions to be caught by the client
         * and because closing a session involves a more significant state change in the program than
         * implied by a typical destructor. If close() can be shown to be exception safe, this protocol may be removed.
         *
         * \return On closing a session, a status object is transferred to the caller.
         */
        Status close();

        /*!
         * \brief Run the configured workflow to completion or error.
         *
         * \return copy of the resulting status.
         *
         * \internal
         * By the time we get to the run() we shouldn't have any unanticipated exceptions.
         * If there are, they can be incorporated into richer future Status implementations
         * or some other more appropriate output type.
         */
        Status run() noexcept;

        /*!
         * \brief Create a new implementation object and transfer ownership.
         *
         * \param context Shared ownership of a Context implementation instance.
         * \param runner MD simulation operation to take ownership of.
         * \param simulationContext Take ownership of the simulation resources.
         * \param logFilehandle Take ownership of filehandle for MD logging
         * \param multiSim Take ownership of resources for Mdrunner multi-sim.
         *
         * \todo Log file management will be updated soon.
         *
         * \return Ownership of new Session implementation instance.
         */
        static std::unique_ptr<SessionImpl> create(std::shared_ptr<ContextImpl>   context,
                                                   std::unique_ptr<gmx::Mdrunner> runner,
                                                   const gmx::SimulationContext  &simulationContext,
                                                   gmx::LogFilePtr                logFilehandle,
                                                   gmx_multisim_t               * multiSim);

        /*! \internal
         * \brief API implementation function to retrieve the current runner.
         *
         * \return non-owning pointer to the current runner or nullptr if none.
         */
        gmx::Mdrunner* getRunner();

        /*!
         * \brief Constructor for use by create()
         *
         * \param context specific context to keep alive during session.
         * \param runner ownership of live Mdrunner object.
         * \param simulationContext take ownership of a SimulationContext
         * \param logFilehandle Take ownership of filehandle for MD logging
         * \param multiSim Take ownership of resources for Mdrunner multi-sim.
         *
         */
        SessionImpl(std::shared_ptr<ContextImpl>   context,
                    std::unique_ptr<gmx::Mdrunner> runner,
                    const gmx::SimulationContext  &simulationContext,
                    gmx::LogFilePtr                logFilehandle,
                    gmx_multisim_t               * multiSim);

    private:
        /*!
         * \brief Extend the life of the owning context.
         *
         * The session will get handles for logging, UI status messages,
         * and other facilities through this interface.
         */
        std::shared_ptr<ContextImpl> context_;

        /*!
         * \brief RAII management of gmx::init() and gmx::finalize()
         *
         * Uses smart pointer to avoid exposing type definition.
         * \todo Not fully implemented.
         */
        std::unique_ptr<MpiContextManager> mpiContextManager_;

        /*!
         * \brief Simulation runner object.
         *
         * If a simulation Session is active, points to a valid Mdrunner object.
         * Null if simulation is inactive.
         */
        std::unique_ptr<gmx::Mdrunner>     runner_;

        /*!
         * \brief An active session owns the resources it is using.
         */
        gmx::SimulationContext simulationContext_;

        /*! \brief Handle to file used for logging.
         *
         * \todo Move to RAII filehandle management; open and close in one place.
         */
        gmx::LogFilePtr logFilePtr_;

        /*!
         * \brief MultiSim resources for Mdrunner instance.
         *
         * May be null for no multi-simulation management at the Mdrunner level.
         */
        gmx_multisim_t* multiSim_;
};

}      //end namespace gmxapi

#endif //GMXAPI_SESSION_IMPL_H