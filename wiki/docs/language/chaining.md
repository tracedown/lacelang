# Request Chaining

Lace scripts can contain multiple HTTP calls that run in sequence. This is how you test workflows that span multiple requests --- login flows, CRUD operations, metric verification, and more.

## Sequential Execution

Calls execute top to bottom, one at a time. Each call completes (including all its chain methods) before the next one starts.

```lace
// Call 1 runs first
get("$BASE_URL/api/status")
.expect(status: 200)

// Call 2 runs after Call 1 finishes
get("$BASE_URL/api/health")
.expect(status: 200)
```

## Passing Data Between Calls

Use `.store()` with `$$var` keys to capture values from one call and use them in the next:

```lace
// Step 1: Log in and capture the token
post("$BASE_URL/auth/login", {
  body: json({ email: "$admin_email", password: "$admin_pass" })
})
.expect(status: 200)
.store({ "$$token": this.body.access_token })

// Step 2: Use the token to access a protected endpoint
get("$BASE_URL/api/profile", {
  headers: { Authorization: "Bearer $$token" }
})
.expect(status: 200)
.store({ "$$user_id": this.body.id })

// Step 3: Use both stored values
delete("$BASE_URL/api/users/$$user_id/sessions", {
  headers: { Authorization: "Bearer $$token" }
})
.expect(status: 204)
```

The `$$token` created in Step 1 is available in Steps 2 and 3. The `$$user_id` created in Step 2 is available in Step 3.

## Verifying Side Effects

A common pattern is to read a value, perform an action, then read again and compare:

```lace
// Read the current count
get("$BASE_URL/api/metrics", {
  headers: { Authorization: "Bearer $api_key" }
})
.expect(status: 200)
.store({ "$$count_before": this.body.event_count })

// Perform an action that should increment the count
post("$BASE_URL/api/events", {
  headers: { Authorization: "Bearer $api_key" },
  body: json({ type: "test_event" })
})
.expect(status: [200, 201])
.wait(2000)

// Read again and verify the count increased
get("$BASE_URL/api/metrics", {
  headers: { Authorization: "Bearer $api_key" }
})
.expect(status: 200)
.store({ "$$count_after": this.body.event_count })
.assert({
  expect: [
    $$count_after eq $$count_before + 1
  ]
})
```

## Cookie-Based Chaining

Cookies persist automatically between calls by default (using the `"inherit"` jar mode). Use named jars to isolate cookie state for different sessions:

```lace
// Admin session
post("$BASE_URL/admin/login", {
  body: json({ email: "$admin_email", password: "$admin_pass" }),
  cookieJar: "named:admin"
})
.expect(status: 200)

// User session (separate cookies)
post("$BASE_URL/login", {
  body: json({ email: "$user_email", password: "$user_pass" }),
  cookieJar: "named:user"
})
.expect(status: 200)

// Admin request (uses admin cookies)
get("$BASE_URL/admin/dashboard", {
  cookieJar: "named:admin"
})
.expect(status: 200)
```

## Delays with .wait()

`.wait()` pauses execution for a specified number of milliseconds after all other chain methods complete. It is always the last chain method.

```lace
post("$BASE_URL/api/jobs", {
  body: json({ type: "reindex" })
})
.expect(status: 202)
.wait(5000)    // wait 5 seconds for the job to complete

get("$BASE_URL/api/jobs/latest")
.expect(status: 200)
.assert({
  expect: [this.body.status eq "completed"]
})
```

!!! note "Hard fail stops everything"
    If a call's `.expect()` or `.assert({ expect: [...] })` fails, all remaining chain methods on that call **and all subsequent calls** are skipped. The script stops at the point of failure.
