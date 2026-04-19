# KLTinyBBS User Guide

This guide is intended for general users and covers how to log in, send and receive private mail, read and post news, and view the user list.

---

## Login and Logout

- **Login or Register**: Enter `/hi username password`
  - Both username and password must be **4–8 printable ASCII** characters (no non-ASCII/Unicode characters).
  - If the account does not exist, it is automatically registered; if it exists, the password is verified.
  - A welcome message is displayed on success; if you have new mail, a ✉️ indicator appears.

- **Logout**: Enter `/bye`
  - Unbinds the current device from the account. You will need to `/hi` again to log back in.

- **Change Password**: Enter `/pass newpassword` while logged in
  - The new password must also be 4–8 printable ASCII characters.

---

## Private Mail (`/m` or `/mail`)

**You must be logged in.**

- **View total count and latest summaries**
  - Enter `/m` or `/mail` (no argument).
  - Returns the total number of messages and a summary of the **latest up to 5** (index, sender, time, content preview).

- **Read the full text of message N**
  - Enter `/m N` or `/mail N` (N is a positive integer matching the summary index).
  - For example, `/m 1` reads the latest message; `/m 2` reads the second latest.

- **List summaries starting from message N**
  - Enter `/m N-` or `/mail N-` (N followed by a minus sign `-`).
  - Lists summaries starting from message N onward, useful for browsing more than 5 messages at once.
  - For example, after `/m` shows the latest 5, enter `/m 6-` to see summaries from message 6 onward.

- **Send a private message**
  - Enter `@username message content` (starts with `@`, followed by the username and message separated by a space).
  - The username must exist; messages exceeding the length limit will be rejected with a notification.

---

## News (`/n` or `/news`) and Posting (`/p` or `/post`)

**You must be logged in.**

- **View total count and latest news summaries**
  - Enter `/n` or `/news`: displays the total count and a summary of the **latest up to 5** posts (index, author, time, content preview).

- **Read the full text of news item N**
  - Enter `/n N` or `/news N` (N is a positive integer).
  - For example, `/n 1` reads the latest news item.

- **List summaries starting from news item N**
  - Enter `/n N-` or `/news N-` (N followed by `-`).
  - Lists summaries starting from item N onward; usage is the same as `/m N-` for mail.

- **Post a news item**
  - Enter `/p content` or `/post content`.
  - Content has a maximum length; exceeding it will result in a rejection notice.

- **Delete a news item**
  - Enter `/delete post N` (N is the index from the `/n` listing; `1` is the latest).
  - Regular users: can only delete **their own posts**.
  - Admin mode: can delete **any user's posts**.

---

## User List (`/users`)

**You must be logged in.**

- Enter `/users`: lists all users (username, last login time).
- Enter `/users keyword`: lists only users whose username **contains** the keyword (case-insensitive).

---

## Other

- **Help**: Enter `/h`, `/help`, or `/?` to get a concise command list. The content varies by context:
  - **Not logged in**: shows only login-related commands (e.g., `/hi username password`).
  - **Logged in**: shows commands available after login (including `/delete post N` for deleting your own posts).
  - **Admin mode**: shows the admin command set (including `/delete post N` for deleting any post).

---

## Quick Reference

| What you want to do | Command example | Notes |
|---------------------|-----------------|-------|
| View mail count + latest 5 summaries | `/m` or `/mail` | No argument |
| Read full text of mail N | `/m N` | N = 1 is the latest |
| List mail summaries from N onward | `/m N-` | N must be followed by `-` |
| View news count + latest 5 summaries | `/n` or `/news` | No argument |
| Read full text of news item N | `/n N` | N = 1 is the latest |
| List news summaries from N onward | `/n N-` | N must be followed by `-` |

All times are displayed in the node's local time. Individual messages and list rows are subject to length limits; the system will notify you when a limit is exceeded.
