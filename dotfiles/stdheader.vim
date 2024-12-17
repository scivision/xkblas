" Modified from https://github.com/42Paris/42header
"
" HOW TO USE
"
"   copy `stdheader.vim` to `$HOME/.vim/plugin`
"   set `USER` environment variable
"   set `MAIL` environment variable
"   press `F2` while in a file

let s:asciiart = [
     \"                       ",
     \"                       ",
     \"                       ",
     \"             *         ",
     \"                   *   ",
     \"     *      .-*-.      ",
     \"          .'* *.'      ",
     \" *     __/_*_*(_     * ",
     \"     / _______ \\      ",
     \"     \\_)     (_/      ",
     \"  *                *   ",
     \"                       ",
\]

let s:start		= '/*'
let s:end		= '*/'
let s:fill		= '*'
let s:length	= 80
let s:margin	= 5

let s:license   = "CeCILL-C"

let s:types		= {
			\'\.c$\|\.h$\|\.cc$\|\.hh$\|\.cpp$\|\.hpp$\|\.tpp$\|\.ipp$\|\.cxx$\|\.go$\|\.rs$\|\.php$\|\.py$\|\.java$\|\.kt$\|\.kts$':
			\['/*', '*/', '*'],
			\'\.htm$\|\.html$\|\.xml$':
			\['<!--', '-->', '*'],
			\'\.js$\|\.ts$':
			\['//', '//', '*'],
			\'\.tex$':
			\['%', '%', '*'],
			\'\.ml$\|\.mli$\|\.mll$\|\.mly$':
			\['(*', '*)', '*'],
			\'\.vim$\|\vimrc$':
			\['"', '"', '*'],
			\'\.el$\|\emacs$\|\.asm$':
			\[';', ';', '*'],
			\'\.f90$\|\.f95$\|\.f03$\|\.f$\|\.for$':
			\['!', '!', '/'],
			\'\.lua$':
			\['--', '--', '-']
			\}

function! s:filetype()
	let l:f = s:filename()

	let s:start	= '#'
	let s:end	= '#'
	let s:fill	= '*'

	for type in keys(s:types)
		if l:f =~ type
			let s:start	= s:types[type][0]
			let s:end	= s:types[type][1]
			let s:fill	= s:types[type][2]
		endif
	endfor

endfunction

function! s:ascii(n)
	return s:asciiart[a:n]
endfunction

function! s:textline(left, right)
	let l:left = strpart(a:left, 0, s:length - s:margin * 2 - strlen(a:right))

	return s:start . repeat(' ', s:margin - strlen(s:start)) . l:left . repeat(' ', s:length - s:margin * 2 - strlen(l:left) - strlen(a:right)) . a:right . repeat(' ', s:margin - strlen(s:end)) . s:end
endfunction

function! s:line(n)
	if a:n == 1 || a:n == 12 " top and bottom line
		return s:start . ' ' . repeat(s:fill, s:length - strlen(s:start) - strlen(s:end) - 2) . ' ' . s:end
	elseif a:n == 2 || a:n == 4 || a:n == 6 || a:n == 9 || a:n == 11  " empty with ascii
		return s:textline('', s:ascii(a:n))
	elseif a:n == 3 " filename
		return s:textline(s:filename(), s:ascii(a:n))
	elseif a:n == 5 " author
		return s:textline("Authors: " . s:user() . " <" . s:mail() . ">", s:ascii(a:n))
	elseif a:n == 7 " created
		return s:textline("Created: " . s:date() . " by " . s:user(), s:ascii(a:n))
	elseif a:n == 8 " updated
		return s:textline("Updated: " . s:date() . " by " . s:user(), s:ascii(a:n))
	elseif a:n == 10 " License
		return s:textline("License: " . s:license, s:ascii(a:n))
	endif
endfunction

function! s:user()
	if exists('g:user')
		return g:user
	endif
	let l:user = $USER
	if strlen(l:user) == 0
		let l:mail = "set 'USER' or 'g:user'"
	endif
	return l:user
endfunction

function! s:mail()
	if exists('g:mail')
		return g:mail
	endif
	let l:mail = $MAIL
	if strlen(l:mail) == 0
		let l:mail = "set 'MAIL' or 'g:mail'"
	endif
	return l:mail
endfunction

function! s:filename()
	let l:filename = expand("%:t")
	if strlen(l:filename) == 0
		let l:filename = "< new >"
	endif
	return l:filename
endfunction

function! s:date()
	return strftime("%Y/%m/%d %H:%M:%S")
endfunction

function! s:insert()
	let l:line = 12

	" empty line after header
	call append(0, "")

	" loop over lines
	while l:line > 0
		call append(0, s:line(l:line))
		let l:line = l:line - 1
	endwhile
endfunction

function! s:update()
	call s:filetype()
	if getline(8) =~ s:start . repeat(' ', s:margin - strlen(s:start)) . "Updated: "
		if &mod
			call setline(8, s:line(8))
		endif
		call setline(3, s:line(3))
		return 0
	endif
	return 1
endfunction

function! s:stdheader()
	if s:update()
		call s:insert()
	endif
endfunction

" Bind command and shortcut
command! Stdheader call s:stdheader ()
map <F2> :Stdheader<CR>
autocmd BufWritePre * call s:update ()
