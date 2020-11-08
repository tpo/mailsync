#ifndef __MAILSYNC_TYPES__

#include <stdio.h>
#include <string>
#include <map>
#include <set>
#include "c-client-header.h"
#include "msgid.h"

using namespace std;

///////////////////////////////// Defines ////////////////////////////////

#define FAILED  false
#define SUCCESS true

//////////////////////////////////////////////////////////////////////////

enum operation_mode_t { mode_unknown, mode_sync, mode_list, mode_diff };

typedef struct MailboxProperties_struct {
  bool no_inferiors;
  bool no_select;
  bool contains_messages;
  bool done;                            // mailbox has been treated (synced...)

  MailboxProperties_struct(): no_inferiors(false),
                              no_select(false),
                              contains_messages(false),
                              done(false) {};
} MailboxProperties;

// we sort our mailboxes by length. That way longer mailboxes and their
// submailboxes (!!) will be traversed first
struct longer
{
  bool operator()(const string& s1, const string& s2) const
  {
    if (s1.length() == s2.length())
      return s1 < s2; // if same length return arbitrary order
                      // - in this case alphabetical
    else
      return s1.length() > s2.length();
  }
};

typedef map<string, MailboxProperties, longer> MailboxMap;
typedef set<MsgId>  MsgIdSet;
typedef map<MsgId, unsigned long> MsgIdPositions;  // Map message ids to
                                                    // positions within a
                                                    // mailbox
typedef map<string, MsgIdSet> MsgIdsPerMailbox;     // A List of message ids
                                                    // per mailbox(-name)

#define __MAILSYNC_TYPES__
#endif
