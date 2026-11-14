#ifndef SWEETBG_DAEMON_FOLLOW_H
#define SWEETBG_DAEMON_FOLLOW_H
struct daemon;
struct sweetbg_follow;
void sweetbg_daemon_follow_init(
	struct sweetbg_follow *follow, struct daemon *daemon);
void sweetbg_daemon_follow_reconcile(
	struct daemon *daemon, struct sweetbg_follow *follow);
#endif
