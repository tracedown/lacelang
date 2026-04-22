# License

The entire canonical Lace stack is licensed under the
**Apache License, Version 2.0**.

| Repository | Description | License |
|------------|-------------|---------|
| [lacelang](https://github.com/tracedown/lacelang) | Specification, grammar, schemas, conformance suite, wiki | Apache 2.0 |
| [lacelang-python-executor](https://github.com/tracedown/lacelang-python-executor) | Reference Python executor (canonical) | Apache 2.0 |
| [lacelang-python-validator](https://github.com/tracedown/lacelang-python-validator) | Reference Python validator (canonical) | Apache 2.0 |
| [lacelang-js-executor](https://github.com/tracedown/lacelang-js-executor) | Reference TypeScript executor (conformant) | Apache 2.0 |
| [lacelang-js-validator](https://github.com/tracedown/lacelang-js-validator) | Reference TypeScript validator (conformant) | Apache 2.0 |
| [lacelang-kt-validator](https://github.com/tracedown/lacelang-kt-validator) | Reference Kotlin validator (conformant) | Apache 2.0 |

You may obtain a copy of the license at:
[apache.org/licenses/LICENSE-2.0](https://www.apache.org/licenses/LICENSE-2.0)

## Responsible use

Lace is designed for legitimate HTTP monitoring, uptime checking, and
API testing against endpoints you **own or have explicit authorization
to probe**.

You are solely responsible for ensuring that your use of Lace and any
executor implementation complies with all applicable laws, regulations,
terms of service, and acceptable use policies.

Using Lace scripts or executor implementations to send unsolicited,
unauthorized, or excessive requests to third-party services — including
but not limited to denial-of-service attacks, unauthorized vulnerability
scanning, or any form of abuse — is strictly prohibited and is the sole
responsibility of the user. The authors, contributors, and maintainers
of this specification and its reference implementations bear no
liability for any misuse.

By using this software, you acknowledge that you have obtained proper
authorization for all endpoints targeted by your probes.
