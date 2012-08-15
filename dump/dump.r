#!/usr/bin/Rscript

x11()

f <- fifo ('fifo', 'r', blocking=TRUE)

y <- matrix(0,16,9)

n <- 0
while (1)
{
	dat <- readLines (f, 9)
	if (length (dat) > 0)
	{
		if (n < 10)
		{
			n <- n + 1
		}
		else
		{
			n <- 0

			dat2 <- strsplit (dat, ' ')
			dat3 <- lapply (dat2, as.numeric)
			for (i in 1:9)
			{
				id <- dat3[[i]][1] + 1
				y[,id] <- dat3[[i]][2:17]
			}

			plot (c(y), type ='h', ylim=c(-0x7ff,0x7ff), xlab='sensor number', ylab='magnetic flux')
		}
	}
}

f.close ()
