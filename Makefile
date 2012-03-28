PORTNAME=	icedtea6
PORTVERSION=	1.10.6
CATEGORIES=	java
MASTER_SITES=	http://icedtea.classpath.org/download/source/:source1 \
		http://icedtea.classpath.org/download/drops/:source2 \
		http://download.java.net/openjdk/jdk6/promoted/b22/:source3
DISTFILES=	icedtea6-${PORTVERSION}.tar.gz:source1 \
		jaxp144_01.zip:source2 \
		jdk6-jaxws-b20.zip:source2 \
		jdk6-jaf-b20.zip:source2 \
		openjdk-6-src-b22-28_feb_2011.tar.gz:source3
EXTRACT_ONLY=	icedtea6-${PORTVERSION}.tar.gz

USE_JAVA=	yes
JAVA_VENDOR=	openjdk
USE_AUTOTOOLS=	autoconf
USE_CONFIGURE=	yes
CONFIGURE_ENV+=	MD5SUM=${MD5} WGET=${TRUE} SHA256SUM=${SHA256}
USE_GNOME=	gnomelibs
LDFLAGS+=	-L${LOCALBASE}/include -L${LOCALBASE}/lib
CPPFLAGS+=	-I${LOCALBASE}/include

CONFIGURE_ARGS=	--with-ecj-jar=${LOCALBASE}/share/java/classes/ecj-3.7.2.jar \
		--with-xalan2-jar=${LOCALBASE}/share/java/classes/xalan.jar \
		--with-xalan2-serializer-jar=${LOCALBASE}/share/java/classes/serializer.jar \
		--with-xerces2-jar=${LOCALBASE}/share/java/classes/xercesImpl.jar \
		--with-rhino=${LOCALBASE}/share/java/rhino/rhino.jar \
		--with-jdk-home=${JAVA_HOME} \
		--with-jaxp-drop-zip=${DISTDIR}/jaxp144_01.zip \
		--with-jaf-drop-zip=${DISTDIR}/jdk6-jaf-b20.zip \
		--with-jaxws-drop-zip=${DISTDIR}/jdk6-jaxws-b20.zip \
		--with-openjdk-src-zip=${DISTDIR}/openjdk-6-src-b22-28_feb_2011.tar.gz

BUILD_DEPENDS=	gcj46:${PORTSDIR}/lang/gcc46 \
		eclipse-ecj>=3.7.2:${PORTSDIR}/java/eclipse-ecj \
		xalan-j>=2.7.1:${PORTSDIR}/textproc/xalan-j \
		rhino>=1.7.r3:${PORTSDIR}/lang/rhino \
		cups-client>=1.5.2:${PORTSDIR}/print/cups-client \
		${LOCALBASE}/libdata/pkgconfig/mozilla-plugin.pc:${PORTSDIR}/www/libxul \
		pkg-config:${PORTSDIR}/devel/pkg-config

LIB_DEPENDS=	jpeg.11:${PORTSDIR}/graphics/jpeg \
		gif.5:${PORTSDIR}/graphics/giflib

post-patch:
	${REINPLACE_CMD} -e 's|--dry-run|-R|g' ${WRKSRC}/Makefile.am ${WRKSRC}/Makefile.in
	${REINPLACE_CMD} -e 's|--check||g' ${WRKSRC}/Makefile.am ${WRKSRC}/Makefile.in

.include <bsd.port.mk>
