# Working with both BN and DDA

Sometimes you may need to port some changes from DDA. It could be done via adding remote and
cherry-picking.

# tl;dr version

```
git remote add dda https://github.com/CleverRaven/Cataclysm-DDA
git fetch dda
git checkout -b ddamaster dda/master
git checkout ddamaster && git pull
```

# Explained version

## Setting up

Assuming you have a directory with BN named `Cataclysm-BN`, open a terminal there.

Add the remote tracking branch for DDA. Let's name the branch `dda` (doesn't need to be `dda`):

```
git remote add dda https://github.com/CleverRaven/Cataclysm-DDA
```

To download the contents of the new branch, it has to be fetched:

```
git fetch dda
```

This downloads all the content from the remote tracking branch. It shouldn't take very long, because
it doesn't download the things common to both repos. Once fetched, you can `merge`, `pull`,
`checkout` etc. the branches on the remote, but you have to prepend them with `dda/`, for example
`dda/master`. It's useful to have a local branch pointing to the remote tracking one:

```
git checkout -b ddamaster dda/master
```

This creates an `ddamaster` branch, which is basically DDA's `master` branch. You can take other
name instead of `ddamaster`.

## Updating

The simplest way is:

```
git checkout ddamaster 
git pull
```

This shouldn't result in any conflicts. If it did, you probably committed changes to the main
branch. In this case you may want to back them up:

```
git checkout -b temp-branch-name
```

And reset the main branch to the remote:

```
git branch -f ddamaster dda/master
```

## Contributing

```
# Switch to BN branch
git checkout main
# Update local content
git pull
# Create new branch for your changes
git checkout -b chainsaw-toothbrush-rebalance
# [do stuff with files]
...
# Commit the changes
git commit -a
# Upload your changes to your fork of Cataclysm (assuming its branch is named origin)
git push -u origin chainsaw-toothbrush-rebalance
# Go to BN's github and make a pull request
```

# Porting

Porting is done with `git cherry-pick`. First, you need to find the hash of the merge commit, or the
first and the last of the commits from the range you want to port. On github, those are on the right
side of commit descriptions in a PR. You can use the full hash or the shortened version. In the
examples, `fafafaf` is the merge commit, while `a0a0a0a` and `b1b1b1b` are the first and last commit
from a range (order is important). Then, from a branch you are porting to, cherry-pick the merge
commit or the range of commits:

```
# Merge commit
git cherry-pick fafafaf
# Commit range
git cherry-pick a0a0a0a..b1b1b1b
```

Resolve the conflicts (there will be many for non-trivial PRs and you'll have to resolve most of
them manually):

```
git mergetool
```

The above may require setting up a merge conflict resolving tool (TODO: describe how to do that).
Then commit and push, as usual.
