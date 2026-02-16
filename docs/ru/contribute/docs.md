# Обновление документации

Чтобы обновить сайт документации, вам нужно:

- [изучить markdown за y минут](https://learnxinyminutes.com/docs/markdown/)
- [создать аккаунт на github](https://github.com/join), так как исходный код сайта документации размещен на github.

## Браузер

![edit page](img/edit.webp)

1. Нажмите кнопку `Edit page` внизу страницы.

![alt text](img/github-edit.webp)

2. Вы будете перенаправлены на страницу github сайта документации. Здесь вы можете редактировать и просматривать ваши изменения.

> [!NOTE]
>
> - `.md` в `CONTRIBUTING.md` означает файлы markdown
> - `.mdx` в `docs.mdx` означает [MarkDown eXtended](https://mdxjs.com)
>   - это надмножество markdown с поддержкой javascript и [компонентов jsx][jsx]
>   - они немного сложнее, но позволяют использовать интерактивные компоненты

[jsx]: https://www.typescriptlang.org/docs/handbook/jsx.html

![propose changes window](https://github.com/scarf005/Cataclysm-BN/assets/54838975/d4a06795-1680-4706-a84c-072346bff109)

3. Нажмите кнопку `Commit changes...` в правом верхнем углу, чтобы [закоммитить ваши изменения](https://github.com/git-guides/git-commit). Убедитесь, что вы:

- Написали короткое и описательное сообщение коммита (`Commit message`)
- Отметили галочкой `Create a new branch for this commit and start a pull request`

![comparing changes page](https://github.com/scarf005/Cataclysm-BN/assets/54838975/3551797e-847b-45fe-8869-8b0b15bfb948)

4. Вы будете перенаправлены на страницу `Comparing changes`. Нажмите кнопку `Create pull request`, чтобы [создать пул-реквест](./contributing.md#примечания-к-пул-реквестам).

![open a pull request page](https://github.com/scarf005/Cataclysm-BN/assets/54838975/2a987c19-b165-43c2-a5a2-639f22202926)

5. Нажмите кнопку `Create pull request`, чтобы [открыть PR](./contributing.md#примечания-к-пул-реквестам). Для небольших изменений можно оставить тело PR пустым.

## Локальная разработка

> [!NOTE]
>
> Этот раздел предполагает, что у вас есть некоторые знания [git](https://git-scm.com) и [javascript](https://developer.mozilla.org/en-US/docs/Web/JavaScript). Конечно, вы можете изучать их по ходу дела.

Чтобы запустить сайт документации локально, вам нужно:

- установить [deno](https://deno.com) для форматирования и генерации автоматической документации

### Настройка сервера разработки

```sh
(Cataclysm-BN) $ deno task docs serve

# или если вы уже находитесь в директории docs
(Cataclysm-BN/docs) $ deno task serve
```

Вы сможете получить доступ к сайту документации по адресу `http://localhost:3000`. Сервер разработки будет автоматически перезагружаться при внесении изменений в документацию.

### Автоматическая генерация страниц

Документация Lua и CLI генерируется автоматически из исходного кода. Чтобы сгенерировать их, перейдите в корень проекта и запустите:

```sh
(Cataclysm-BN) $ deno task docs:gen
```

## Лицензия

- Внося вклад в файлы markdown (включая, но не ограничиваясь файлами `.md` и `.mdx`), вы соглашаетесь лицензировать свои вклады под [CC-BY-SA 3.0](https://creativecommons.org/licenses/by-sa/3.0/), той же лицензией, что и игра.

- Внося вклад в исходный код страницы документации (включая, но не ограничиваясь файлами `.ts`), вы соглашаетесь лицензировать свои вклады под [AGPL 3.0](https://www.gnu.org/licenses/agpl-3.0.en.html).
