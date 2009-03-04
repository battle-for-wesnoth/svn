EAPI=2
inherit eutils toolchain-funcs flag-o-matic games

MY_PV=${PV/_/}
DESCRIPTION="Battle for Wesnoth - A fantasy turn-based strategy game"
HOMEPAGE="http://www.wesnoth.org/"
SRC_URI="mirror://sourceforge/wesnoth/${PN}-${MY_PV}.tar.bz2"

LICENSE="GPL-2"
SLOT="0"
KEYWORDS="~amd64 ~ppc ~ppc64 ~sparc ~x86 ~x86-fbsd"
IUSE="dedicated +editor lite nls server smallgui tinygui tools"

RDEPEND=">=media-libs/libsdl-1.2.7
	media-libs/sdl-net
	dev-libs/boost
	!dedicated? (
		x11-libs/libX11
		>=media-libs/libsdl-1.2.7[X]
		>=media-libs/sdl-mixer-1.2[vorbis]
		>=media-libs/sdl-image-1.2[png,jpeg]
		>=media-libs/sdl-ttf-2.0.8
		x11-libs/pango
	)
	nls? ( virtual/libintl )"
DEPEND="${RDEPEND}
	!dedicated? (
		smallgui? ( media-gfx/imagemagick )
		tinygui? ( media-gfx/imagemagick )
	)
	nls? ( sys-devel/gettext )
	>=dev-util/scons-0.96.93"

S=${WORKDIR}/${PN}-${MY_PV}

pkg_setup() {
	if use !dedicated && use smallgui && use tinygui ; then
		ewarn "USE=tinygui overrides USE=smallgui"
		ebeep
		epause 10
	fi
	games_pkg_setup
}

src_unpack() {
	unpack ${A}
	if use dedicated || use server ; then
		sed \
			-e "s:GAMES_BINDIR:${GAMES_BINDIR}:" \
			-e "s:GAMES_STATEDIR:${GAMES_STATEDIR}:" \
			-e "s/GAMES_USER_DED/${GAMES_USER_DED}/" \
			-e "s/GAMES_GROUP/${GAMES_GROUP}/" "${FILESDIR}"/wesnothd.rc \
			> "${T}"/wesnothd \
			|| die "sed failed"
	fi
}

src_configure() {
	true
}

src_compile() {
	local myconf

	local SCONSOPTS=`echo ${MAKEOPTS} | \
		sed s/-j$/-j256/\;s/-j[[:space:]]/-j256\ / |
		sed s/--jobs$/--jobs=256/\;s/--jobs[[:space:]]/--jobs=256\ /`

	einfo "running scons with ${SCONSOPTS}"

	filter-flags -ftracer -fomit-frame-pointer
	if [[ $(gcc-major-version) -eq 3 ]] ; then
		filter-flags -fstack-protector
		append-flags -fno-stack-protector
	fi
	if use dedicated || use server ; then
		myconf="${myconf} wesnothd"
		myconf="${myconf} campaignd"
		myconf="${myconf} server_uid=${GAMES_USER_DED}"
		myconf="${myconf} server_gid=${GAMES_GROUP}"
	fi
	if use !dedicated ; then
		myconf="${myconf} wesnoth"
	fi
	use tools && myconf="${myconf} cutter exploder"
	if use editor; then
		myconf="${myconf} editor=yes"
	else
		myconf="${myconf} editor=no"
	fi
	if use tinygui ; then
		myconf="${myconf} gui=tiny"
	elif use smallgui ; then
		myconf="${myconf} gui=small"
	fi
	if use lite ; then
		myconf="${myconf} lowmem=true"
	fi
	if use nls ; then
		myconf="${myconf} nls=true"
	else
		myconf="${myconf} nls=false"
	fi

	if has ccache $FEATURES; then
		myconf="${myconf} ccache=yes"
	else
		myconf="${myconf} ccache=no"
	fi

	scons $myconf \
		${SCONSOPTS/-l[0-9]} \
		localedirname=/usr/share/locale \
		prefix=/usr/games \
		prefsdir=.wesnoth-1.5 \
		icondir=/usr/share/icons \
		desktopdir=/usr/share/applications \
		docdir=/usr/share/doc/${PF} \
		default_targets=none ||
		die "scons build failed"
}

src_install() {
	scons install destdir=${D} --implicit-deps-unchanged ||
	die "scons install failed"
	dodoc changelog
	if use dedicated || use server; then
		keepdir "${GAMES_STATEDIR}/run/wesnothd"
		doinitd "${T}"/wesnothd || die "doinitd failed"
	fi
	prepgamesdirs
}
