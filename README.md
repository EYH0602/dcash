# dcash Wallet Server

## Background

This project is a continuation of the ECS 150 Sprint 2021 Project 3 and 4
[gunrock server](https://github.com/kingst/gunrock_web).
For the course, each project have different focus
* project 3: multi-threaded concurrency web-server. For details see [doc for gunrock web server](docs/gunrock_web.md).
* project 4: http API-server interacting with [Stripe](https://stripe.com/) and a simple cli client.
  For details see [doc for dcash wallet](docs/dcash.md).

## What's new

After the course is finished, I fill this project is not finished properly.

1. This in-memory STL DB is not safe and reliable.
2. no concurrency for the second part.
3. the usability of the cli client is low.

So here comes this db-integration and concurrency support.

And a web based client can be found [here]().
