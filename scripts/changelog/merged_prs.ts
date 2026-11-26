import { minBy } from "jsr:@std/collections"
import { botRe, type Commit } from "./read_commits.ts"

export type MergedPr = { number: number; title: string }
/** `prs` maps merge commit SHAs to PRs. `prNumbers` holds the `(#N)` suffixes that are PRs. */
export type PrIndex = { prs: Map<string, MergedPr>; prNumbers: Set<number> }

const token = () => Deno.env.get("GH_TOKEN") ?? Deno.env.get("GITHUB_TOKEN")

// deno-lint-ignore no-explicit-any
const api = async (path: string, body?: unknown): Promise<any> => {
  const res = await fetch(`https://api.github.com/${path}`, {
    method: body ? "POST" : "GET",
    headers: token() ? { Authorization: `Bearer ${token()}` } : {},
    body: body ? JSON.stringify(body) : undefined,
  })
  if (res.status === 404) return null
  const json = await res.json()
  if (!res.ok || json.errors && !json.data) {
    throw new Error(`${path}: ${res.status} ${JSON.stringify(json)}`)
  }
  return json
}

export const suffixOf = (commit: Commit): number | undefined => {
  const match = commit.subject.match(/\(#(\d+)\)$/u)
  return match ? Number(match[1]) : undefined
}

/** One GraphQL request per 100 PRs. Needs a token. */
const fetchGraphql = async (repo: string, since: Date, numbers: number[]): Promise<PrIndex> => {
  const [owner, name] = repo.split("/")
  const index: PrIndex = { prs: new Map(), prNumbers: new Set() }
  for (let after = null, first = true;; first = false) {
    const lookups = first
      ? numbers.map((n) => `n${n}: issueOrPullRequest(number: ${n}) { __typename }`)
      : []
    const { data: { repository } } = await api("graphql", {
      query: `query($owner: String!, $name: String!, $after: String) {
        repository(owner: $owner, name: $name) {
          ${lookups.join("\n")}
          pullRequests(states: MERGED, baseRefName: "main", first: 100, after: $after,
                       orderBy: { field: UPDATED_AT, direction: DESC }) {
            pageInfo { hasNextPage endCursor }
            nodes { number title updatedAt mergeCommit { oid } }
          }
        }
      }`,
      variables: { owner, name, after },
    })
    for (const n of numbers) {
      if (repository[`n${n}`]?.__typename === "PullRequest") index.prNumbers.add(n)
    }
    const { nodes, pageInfo } = repository.pullRequests
    for (const pr of nodes) if (pr.mergeCommit) index.prs.set(pr.mergeCommit.oid, pr)
    if (!pageInfo.hasNextPage || new Date(nodes.at(-1).updatedAt) < since) return index
    after = pageInfo.endCursor
  }
}

/** Works without a token: 2.4 MB per 100 PRs, plus one request per unknown `(#N)`. */
const fetchRest = async (repo: string, since: Date, numbers: number[]): Promise<PrIndex> => {
  const prs = new Map<string, MergedPr>()
  for (let page = 1;; page++) {
    const pulls = await api(
      `repos/${repo}/pulls?state=closed&base=main&sort=updated&direction=desc&per_page=100&page=${page}`,
    )
    for (const pr of pulls) if (pr.merged_at) prs.set(pr.merge_commit_sha, pr)
    if (pulls.length < 100 || new Date(pulls.at(-1).updated_at) < since) break
  }
  const known = new Set([...prs.values()].map((x) => x.number))
  const issues = await Promise.all(
    numbers.filter((n) => !known.has(n)).map((n) => api(`repos/${repo}/issues/${n}`)),
  )
  const prNumbers = known.union(new Set(issues.filter((x) => x?.pull_request).map((x) => x.number)))
  return { prs, prNumbers }
}

/** Fetch the PRs merged since the oldest commit. Uses GraphQL when a token is set. */
export const fetchPrIndex = (
  { repo, commits }: { repo: string; commits: Commit[] },
): Promise<PrIndex> | PrIndex => {
  const since = minBy(commits, (x) => x.committed)?.committed
  if (!since) return { prs: new Map(), prNumbers: new Set() }

  const numbers = [...new Set(commits.map(suffixOf).filter((n) => n !== undefined))]
  return (token() ? fetchGraphql : fetchRest)(repo, since, numbers)
}

/**
 * Replace the commits of each rebase-merged PR with one commit titled `<PR title> (#N)`.
 * The commits of a PR are the commits after the previous PR's merge commit, up to and
 * including its own merge commit. Keep a squash merge commit, which ends in its own `(#N)`.
 * Inside a rebase merge, keep a commit separate only when its `(#N)` is in `prNumbers`,
 * because `(#N)` can be an issue number.
 * Commits are newest first, as git log prints them.
 */
export const groupRebaseMerges = (
  { commits, prs, prNumbers }: PrIndex & { commits: Commit[] },
): Commit[] => {
  const result: Commit[] = []
  let group: Commit[] = []

  for (const commit of commits.toReversed()) {
    group.push(commit)
    const pr = prs.get(commit.sha)
    if (!pr) continue

    if (suffixOf(commit) === pr.number) {
      result.push(...group)
    } else {
      const isOtherPr = (x: Commit) => prNumbers.has(suffixOf(x)!) && suffixOf(x) !== pr.number
      const own = group.filter((x) => !isOtherPr(x))
      const people = [...new Set(own.flatMap((x) => [x.author, ...x.coauthors]))]
        .filter((x) => !botRe.test(x))
      result.push(...group.filter(isOtherPr), {
        ...commit,
        subject: `${pr.title} (#${pr.number})`,
        author: people[0] ?? commit.author,
        coauthors: people.slice(1),
      })
    }
    group = []
  }
  result.push(...group)
  return result.toReversed()
}
