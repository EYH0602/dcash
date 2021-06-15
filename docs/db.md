# Database Integration for Dcash Wallet API Server

This project is an extension of the ECS 150 project: Dcash Wallet.
In the original design of Dcash Wallet keeps all the data in memory (STL) for simplicity.
However, the origional design had some problems:
* The server does not save the data. When it is shut down, all the data are gone.
* There is no simple to interact with other platform and business.
    * For example, we may need to send data to HIVE and consumers.

## Dcash API Server Data-structure

This new design of the `Database` class communicate with a MySQL database to do the data management.

DB name: `dcash`.

There will be three tables in the DB:
* `users` keeps all the user account record.
* `transfers` keeps record of all transfers between account.
* `deposits` keeps record of all deposit to the accounts.

**NOTE**.
`transfers` may be stored in a graph DB in the future.

### users

This `users` table is to keep record of all the user account information.
All users will initially be created with empty email and 0 balance.

```mysql
CREATE TABLE `users` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `username` varchar(255) NOT NULL,
  `password` varchar(255) NOT NULL,
  `user_id` varchar(255) NOT NULL COMMENT 'randomized string',
  `email` varchar(255) NOT NULL DEFAULT '',
  `balance` bigint(20) NOT NULL DEFAULT 0,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=0 DEFAULT CHARSET=utf8 COMMENT='Account info of users'
```

### transfers

This `transfers` table keeps record of all transfer history.
Each row is a history record, knowing the sender, receiver, and amount they send.

```mysql
CREATE TABLE `transfers` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `from` varchar(255) NOT NULL,
  `to` varchar(255) NOT NULL,
  `amount` bigint(20) NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=0 DEFAULT CHARSET=utf8 COMMENT='Transfer records'
```

These information might be stored in a graph DB in the future,
since the current design of table will store extra information that are not needed.
For example, if there is a person Steve keep sending money to other people,
the table would be like

|id|from|to|amount|
|---|---|---|---|
|1|Steven|A|1000|
|2|Steven|A|2000|
|3|Steven|B|3000|
|4|Steven|C|4000|

```mermaid
graph TD;
    Steven-->A;
    Steven-->A;
    Steven-->B;
    Steven-->C;
```

### deposits

This `deposits` table user to keep record of all deposits to user accounts.
According to the API, for each deposit API call,
there need to exist an additional `stripe_charge_id` representing the credit card
so that our API server could make API call to Stripe to charge money to the card.

```mysql
CREATE TABLE `deposits` (
  `id` bigint(20) unsigned NOT NULL AUTO_INCREMENT,
  `to` varchar(255) NOT NULL,
  `amount` bigint(20) NOT NULL,
  `stripe_charge_id` varchar(255) NOT NULL,
  PRIMARY KEY (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=0 DEFAULT CHARSET=utf8 COMMENT='Deposit records'
```
