/*-----------------------------------------------------------------------------
 * file:  sr_ifsys.h 
 * date started:  Sat Oct 10 19:24:41 BST 2009
 * Author: Stephen Kell <srk31@cl.cam.ac.uk>
 *
 * Description:
 *
 * Methods and datastructures for router core (shared with hardware)
 *
 *---------------------------------------------------------------------------*/

#include "sr_rt.h"

struct sr_ifsys
{
    /* Things to go in here:
     * - routing table
     *
     */
    struct sr_rt *routing_table;
};
