## Reviewing Pull Requests

- Do NOT write overall summaries, general assessments, or concluding remarks about the PR.

### Code Review Style

- Comment only on objective and obvious mistakes.
- All comments must be direct, related to given PR, and **actionable**.
- Avoid meta-commentary about the PR as a whole.

### Guidelines for `suggestion` Blocks

Because with `suggestion` block allows the author to commit a change with a single click, it **MUST ONLY** be used when the proposed change is entirely **SELF-CONTAINED** and requires no further edits to be complete.

#### When to use `suggestion` blocks (DO):

- **Description:** Use for changes that are complete within the suggested block, such as fixing a typo, simplifying logic, or correcting a single function call.
- **Example:**
  ````markdown
  We can simplify this by removing the `if/else` statement.

  ```suggestion
  - if (condition) {
  -     return true;
  - } else {
  -     return false;
  - }
  + return condition;
  ```
  ````

#### When NOT to use `suggestion` blocks (DON'T):

- **Scenario 1:** The suggestion relies on new variables or functions that are not defined within the suggestion block itself.
- **Correct Way to Comment (use a standard code block, not a `suggestion`):**
  ````markdown
  This can be clarified by using the predefined enum values.

  ```c++
  const foo = burning ? BURNING_VALUE : NOT_BURNING_VALUE;
  ```
  ````

- **Scenario 2:** The required change spans multiple files. A `suggestion` block can only modify the code it is attached to.
- **Incorrect Usage (This is what you should AVOID):**
  ````markdown
  This variable name has a typo and needs to be fixed in 10 other files as well.

  ```suggestion
  - const a_very_important_varible = 42;
  + const a_very_important_variable = 42;
  ```
  ````
