# Game Objects

Many of the things that physically exist within the game world are **game objects(GO)**. Currently
this only covers items, but creatures and vehicles are coming soon and then probably furniture at
some point. GOs have a small common interface with methods like name and position that you can find
in `game_object.h`. GOs use private constructors, this means **your variables must always be
references or pointers**. Trying to assign a variable without a layer of indirection like that will
cause a compile error. Likewise trying to copy one object over another will cause a similar error.

```cpp
item& it_ref = ...; // Good
item* it_pointer; // Good

item it = ...; // Compile error
it_ref = other; // Compile error
*it_pointer = other; // Compile error
```

New GOs can be created via `::spawn` static methods that return a `detached_ptr` to the newly
created game object. A `detached_ptr` represents an object that does not currently exist in the game
world. There can only be one `detached_ptr` to an object at a time (those who know
[`std::unique_ptr`](https://en.cppreference.com/w/cpp/memory/unique_ptr) will be familiar with this
behavior). Functions that add an object to the world will require that you `std::move` in a
`detached_ptr`. In general `detached_ptr`s must be `std::move`'d when they're passed into a
function. Note that you should never use any variable after it has been moved. Likewise functions
that remove an object from the world will return a `detached_ptr`. This ensures that you can't add
the same thing to the world twice.

Functions that don't add things to the world just accept normal pointers. You can turn a
`detached_ptr` into a normal pointer by doing this. Note that you can use this to keep a valid
pointer to something even after you `std::move` it, but you must do this before the `std::move`
happens.

```cpp
&*detached
```

And you can go the other way using this. Though note that it removes the object from the game world
in the process and will cause errors if called on an object that isn't in the game world.

```cpp
detached_ptr<item> as_detached = normal_ptr->detach();
```

Trying to access an invalid `detached_ptr` (for instance one that has been `std::move`'d from) will
cause a debugmsg and give you the null version of that object.

## Safe References

Game objects support safe references. `safe_reference<item>` will refuse access to an object that
has been destroyed or is outside of the reality bubble without losing track of it, and they can be
saved and loaded. You must check them as a boolean (e.g. `if( ref )`) to see if they're valid.
Trying to access them when they're not will cause debugmsgs and give you a null object instead. They
have a small interface that lets you check if they're destroyed or unloaded etc. If they were
destroyed or unloaded this turn, they have a method that will allow you to access the object in a
const fashion, for instance to display an error message.

If you're moving objects around you need to use `detached_ptr`s, but otherwise when choosing which
reference to use the most important thing to consider is how long you want to hold onto it. If
you're just using something temporarily, for instance most arguments and variables, you should use a
normal reference or pointer. If you need to store it for longer you should use a safe reference and
this means it can be easily stored in the save file. In the rare case that you do want to save it
across turns but don't want to store it in the save file, which means caches, there's also a fast
`cache_reference<item>`, which does last across turns but can't be saved.

Game objects can sometimes be split into pieces or merged together. Item stacks are the main example
of this but there are others like vehicles being split or dissoluted devourers merging. When a stack
of items is split, the stack that stays in place is the one that safe references will follow. When
they are merged safe references to either half of the merge will now point to the merge result.
