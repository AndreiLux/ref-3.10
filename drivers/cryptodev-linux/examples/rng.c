/*
 * Demo on how to use /dev/crypto device for random number generation.
 *
 * Placed under public domain.
 *
 */
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <crypto/cryptodev.h>
#include <stdint.h>

int rng_init(int cfd, int *session_out, uint8_t *key, unsigned int key_size)
{
#ifdef CIOCGSESSINFO
	struct session_info_op siop;
#endif
	struct session_op session = {
		.rng = CRYPTO_ANSI_CPRNG,
		.key = key,
		.keylen = key_size
	};

	if (ioctl(cfd, CIOCGSESSION, &session)) {
		perror("ioctl(CIOCGSESSION)");
		return -1;
	}

#ifdef CIOCGSESSINFO
	siop.ses = session.ses;
	if (ioctl(cfd, CIOCGSESSINFO, &siop)) {
		perror("ioctl(CIOCGSESSINFO)");
		return -1;
	}
	printf("Got %s with driver %s\n",
			siop.rng_info.cra_name, siop.rng_info.cra_driver_name);
	if (!(siop.flags & SIOP_FLAG_KERNEL_DRIVER_ONLY)) {
		printf("Note: This is not an accelerated cipher\n");
	}
	/*printf("Alignmask is %x\n", (unsigned int)siop.alignmask);*/
#endif
	*session_out = session.ses;
	return 0;
}

void rng_deinit(int cfd, int session)
{
	if (ioctl(cfd, CIOCFSESSION, &session)) {
		perror("ioctl(CIOCFSESSION)");
	}
}

int
rng_get_bytes(int cfd, int session, void *out, size_t outsize)
{
	struct crypt_op cryp = {
		.ses = session,
		.dst = out,
		.len = outsize
	};
	void *p;

	if (ioctl(cfd, CIOCCRYPT, &cryp)) {
		perror("ioctl(CIOCCRYPT)");
		return -1;
	}

	return 0;
}

int
main()
{
	int cfd = -1, session, i;
	uint8_t randoms[20];
	const size_t randomslen = sizeof(randoms)/sizeof(*randoms);
	char text[] = "The quick brown fox jumps over the lazy dog 1,234,567,890 times.";

	/* Open the crypto device */
	cfd = open("/dev/crypto", O_RDWR, 0);
	if (cfd < 0) {
		perror("open(/dev/crypto)");
		return 1;
	}

	/* Set close-on-exec (not really neede here) */
	if (fcntl(cfd, F_SETFD, 1) == -1) {
		perror("fcntl(F_SETFD)");
		return 1;
	}

	rng_init(cfd, &session, text, 48);
	rng_get_bytes(cfd, session, randoms, randomslen);
	rng_deinit(cfd, session);

	printf("random bytes: ");
	for (i = 0; i < randomslen; i++) {
		printf("%02x", randoms[i]);
	}
	printf("\n");

	/* Close the original descriptor */
	if (close(cfd)) {
		perror("close(cfd)");
		return 1;
	}

	return 0;
}
