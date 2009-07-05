/*****************************************************************************
 *
 *             Copyright (c) 2009 Sony Pictures Imageworks, Inc.
 *                            All rights reserved.
 *
 *  This material contains the confidential and proprietary information
 *  of Sony Pictures Imageworks, Inc. and may not be disclosed, copied or
 *  duplicated in any form, electronic or hardcopy, in whole or in part,
 *  without the express prior written consent of Sony Pictures Imageworks,
 *  Inc. This copyright notice does not imply publication.
 *
 *****************************************************************************/


#include <vector>
#include <string>
#include <cstdio>

#include "OpenImageIO/dassert.h"
#include "OpenImageIO/thread.h"

#include "oslexec_pvt.h"



namespace OSL {
namespace pvt {   // OSL::pvt



ShadingExecution::ShadingExecution ()
    : m_context(NULL), m_instance(NULL), m_master(NULL),
      m_bound(false)
{
}



ShadingExecution::~ShadingExecution ()
{
}



void
ShadingExecution::bind (ShadingContext *context, ShaderUse use,
                        int layerindex, ShaderInstance *instance)
{
    ASSERT (! m_bound);  // avoid double-binding
    ASSERT (context != NULL && instance != NULL);

    std::cerr << "bind ctx " << (void *)context << " use " 
              << shaderusename(use) << " layer " << layerindex << "\n";
    m_use = use;

    // Take various shortcuts if we are re-binding the same instance as
    // last time.
    bool rebind = (m_context == context && m_instance == instance);
    if (! rebind) {
        m_context = context;
        m_instance = instance;
        m_master = instance->master ();
        ASSERT (m_master);
    }

    m_npoints = m_context->npoints ();

    // FIXME: bind the symbols -- get the syms ready and pointing to the
    // right place in the heap,, interpolate primitive variables, handle
    // connections, initialize all parameters

    m_bound = true;
    m_executed = false;
}



void
ShadingExecution::run (Runflag *rf)
{
    if (m_executed)
        return;       // Already executed

    std::cerr << "Running ShadeExec " << (void *)this << ", shader " 
              << m_master->shadername() << "\n";

    ASSERT (m_bound);  // We'd better be bound at this point

    // Make space for new runflags
    m_runflags = (Runflag *) alloca (m_npoints * sizeof(Runflag));
    if (rf) {
        // Passed runflags -- copy those
        memcpy (m_runflags, rf, m_npoints*sizeof(Runflag));
        // FIXME -- restrict begin/end
        m_beginpoint = 0;
        m_endpoint = m_npoints;
        m_allpointson = true;
    } else {
        // If not passed runflags, make new ones
        for (int i = 0;  i < m_npoints;  ++i)
            m_runflags[i] = 1;
        m_beginpoint = 0;
        m_endpoint = m_npoints;
        m_allpointson = true;
    }

    // FIXME -- push the runflags, begin, end

    // FIXME -- this runs every op.  Really, we just want the main code body.
    run (0, (int)m_master->m_ops.size());

    // FIXME -- pop the runflags, begin, end

    m_executed = true;
}



void
ShadingExecution::run (int beginop, int endop)
{
    std::cerr << "Running ShadeExec " << (void *)this 
              << ", shader " << m_master->shadername() 
              << " ops [" << beginop << "," << endop << ")\n";
    for (m_ip = beginop; m_ip < endop && m_beginpoint < m_endpoint;  ++m_ip) {
        Opcode &op = m_master->m_ops[m_ip];
        std::cerr << "  instruction " << m_ip << ": " << op.opname() << " ";
        for (int i = 0;  i < op.nargs();  ++i) {
            int arg = m_master->m_args[op.firstarg()+i];
            std::cerr << m_instance->symbol(arg)->mangled() << " ";
        }
        std::cerr << "\n";
        ASSERT (op.implementation() && "Unimplemented op!");
        op (this, op.nargs(), &m_master->m_args[op.firstarg()],
            m_runflags, m_beginpoint, m_endpoint);
    }
    // FIXME -- this is a good place to do all sorts of other sanity checks,
    // like seeing if any nans have crept in from each op.
}



}; // namespace pvt
}; // namespace OSL